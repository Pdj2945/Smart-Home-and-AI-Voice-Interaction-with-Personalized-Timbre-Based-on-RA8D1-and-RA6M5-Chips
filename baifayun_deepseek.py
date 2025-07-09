#!/usr/bin/env python
# -*- coding: utf-8 -*-

import socket
import time
import threading
import json
import requests
import urllib.parse  # 用于处理URL解码
import re  # 用于正则表达式
from collections import deque  # 用于实现队列功能

# DeepSeek API配置
DEEPSEEK_API_KEY = "sk-2c0aefc9bb9b49af9071b0d763dfc9e8"
DEEPSEEK_API_URL = "https://api.deepseek.com/v1/chat/completions"

# 巴法云TCP云平台配置
BAFA_HOST = "bemfa.com"
BAFA_PORT = 8344
BAFA_KEY = "11ca31c562d14ef786a0238a03a3be16"  # 巴法云用户密钥
BAFA_TOPIC_TTS_STT = "text1"  # 用于TTS和STT的主题
BAFA_TOPIC_SENSOR_CONTROL = "text2"  # 用于传感器数据和控制指令的主题
BAFA_TOPIC_RESERVED = "text3"  # 预留主题
HEARTBEAT_INTERVAL = 60  # 心跳间隔（秒），巴法云要求60秒发送一次心跳
RECONNECT_INTERVAL = 5   # 重连间隔时间（秒）
MAX_RECONNECT_ATTEMPTS = 5  # 最大重连尝试次数

# 分段消息配置
MAX_SEGMENT_LENGTH = 40  # 每段最大字符数
STT_PREFIX = "stt"      # 语音识别结果前缀
TTS_PREFIX = "tts"      # 文字转语音前缀
SEGMENT_DELAY = 0.5     # 分段消息发送间隔(秒)

# 报警阈值配置
TEMP_HIGH_THRESHOLD = 40.0  # 温度过高阈值（摄氏度）
TEMP_LOW_THRESHOLD = 10.0   # 温度过低阈值（摄氏度）
FLAME_THRESHOLD = 500       # 火焰传感器阈值（ADC0，值越小越危险）
SMOKE_THRESHOLD = 500       # 烟雾传感器阈值（ADC4，值越小越危险）
ALARM_CONSECUTIVE_COUNT = 3  # 连续超过阈值的次数才触发报警
ALARM_COOLDOWN = 60         # 报警冷却时间（秒）

# 控制指令配置
CONTROL_AI_PROMPT = """你是一个智能家居和小车控制系统的AI助手，你的任务是从用户的自然语言中识别出可能的控制指令。
你需要识别以下类型的控制指令：
1. LED控制: 开灯/关灯/闪烁灯光
2. 蜂鸣器控制: 开启蜂鸣器/关闭蜂鸣器/蜂鸣器间歇鸣叫/SOS模式
3. 工作模式: 自动模式/手动模式
4. 小车导航: 识别用户想要小车前往的位置名称，如"客厅"、"厨房"、"卧室"等
5. 特殊功能: 寻找小车/求救

请根据用户的语言，输出对应的控制指令，格式为：
控制指令:LED=ON/OFF/BLINK|BUZZER=ON/OFF/BEEP/SOS|MODE=AUTO/MANUAL|NAVIGATE=位置名称|FIND=ON|HELP=ON
如果有多个控制指令，请用竖线"|"分隔。
如果没有检测到控制指令，请输出：控制指令:null

对于特殊功能，请注意：
1. 当用户表达想要寻找小车的意图时（如"小车在哪里"、"找不到小车"、"定位小车"等），输出FIND=ON
2. 当用户表达求救或紧急情况时（如"救命"、"紧急情况"、"需要帮助"等），输出HELP=ON

对于小车导航指令，请特别注意：
1. 当用户表达想要小车去某个位置时，输出NAVIGATE=位置名称
2. 位置名称应该是用户提到的具体位置，如"客厅"、"厨房"等
3. 如果用户提到多个任务，如"去客厅测量温度"，应该识别出导航指令和后续任务

例如：
输入："房间好暗，看不清东西"
输出："控制指令:LED=ON"

输入："这个蜂鸣器声音太吵了"
输出："控制指令:BUZZER=OFF"

输入："房间太暗了，能不能开灯并让蜂鸣器响一下提醒我"
输出："控制指令:LED=ON|BUZZER=BEEP"

输入："小车，去客厅看看"
输出："控制指令:NAVIGATE=客厅"

输入："能麻烦你去厨房测量一下温度吗？"
输出："控制指令:NAVIGATE=厨房"

输入："请前往卧室并开灯"
输出："控制指令:NAVIGATE=卧室|LED=ON"

输入："小车在哪里？我找不到它了"
输出："控制指令:FIND=ON"

输入："救命！这里有紧急情况"
输出："控制指令:HELP=ON"

输入："我想看看天气怎么样"
输出："控制指令:null"
"""

# AI助手提示词 - 可根据需要修改
AI_PROMPT = """你是一个智能AI助手，请提供简洁、有帮助的回答。
- 保持友好和礼貌
- 不使用表情符号
- 当用户要求特定字数的回复时，必须严格按照要求的字数回复
- 逗号使用英文逗号','句号也使用英文句号'.'

你控制着一个智能家居系统和一个UWB定位小车。小车可以在不同区域间移动，并执行任务。

如果你收到的消息格式为"控制:XXX(没有就是null),原文本:YYY"，这表示:
- 控制部分表示系统已经执行的控制指令
- 原文本部分是用户的原始问题

当你看到控制部分不为null时，你应该在回复中首先确认已执行的控制操作，然后再回答用户的问题。

控制指令可能包括:
1. LED控制: LED=ON/OFF/BLINK
2. 蜂鸣器控制: BUZZER=ON/OFF/BEEP/SOS
3. 工作模式: MODE=AUTO/MANUAL
4. 小车导航: NAVIGATE=位置名称
5. 特殊功能: FIND=ON (寻找小车) 或 HELP=ON (紧急求救)

当你收到报警信息时，请务必以严肃认真的态度回应：
- 温度过高报警 (>40°C): 提醒用户注意温度过高可能导致的危险，如设备过热、火灾风险等
- 温度过低报警 (<10°C): 提醒用户注意温度过低可能导致的问题，如水管冻结、室内不适等
- 火焰报警: 这是严重的安全隐患，提醒用户可能有火灾风险，建议立即检查
- 烟雾报警: 这是严重的安全隐患，提醒用户可能有火灾或气体泄漏风险，建议立即检查

例如:
- 如果收到"控制:LED=ON,原文本:房间好暗"，你应该回复类似"我已经帮您打开了灯光。房间现在应该更亮了，您还需要其他帮助吗？"
- 如果收到"控制:NAVIGATE=客厅,原文本:去客厅测量温度"，你应该回复类似"我已经指示小车前往客厅测量温度，稍后会告诉您结果。"
- 如果收到"控制:FIND=ON,原文本:小车在哪里"，你应该回复类似"我已经激活了寻找模式，小车正在闪烁灯光并发出声音，请查看周围环境。"
- 如果收到"控制:HELP=ON,原文本:救命"，你应该回复类似"我已经激活了紧急求救模式，小车正在发出SOS信号。请保持冷静，如需要请联系紧急服务。"
- 如果收到"报警:火焰报警,原文本:null"，你应该回复类似"警告！系统检测到火焰信号，这可能表明有火灾风险。请立即检查环境安全，确认是否需要采取紧急措施。"

当小车到达目的地后，会发送一个通知，此时你应该告知用户小车已到达并正在执行任务。
"""

class BafaCloudClient:
    def __init__(self):
        self.socket = None
        self.connected = False
        self.running = False
        self.reconnect_count = 0
        self.last_heartbeat_time = 0
        self.chat_history = []  # 用于保存对话历史
        self.recent_replies = []  # 用于保存最近的回复，防止回环
        self.recent_replies_max_size = 5  # 保存最近5个回复
        
        # 分段消息处理
        self.segmented_messages = {}  # 用于存储接收到的分段消息，格式: {prefix: {total_segments, segments: {idx: content}}}
        
        # 传感器数据队列
        self.sensor_data_queue = deque(maxlen=10)  # 最多保存10条最近的传感器数据
        self.control_chat_history = []  # 用于控制指令AI的对话历史
        
        # 控制指令状态跟踪
        self.last_command = None  # 最后发送的控制指令
        self.command_history = deque(maxlen=20)  # 保存最近20条控制指令历史
        
        # 报警状态跟踪
        self.alarm_status = {
            'high_temp': {'count': 0, 'active': False, 'last_alarm': 0},
            'low_temp': {'count': 0, 'active': False, 'last_alarm': 0},
            'flame': {'count': 0, 'active': False, 'last_alarm': 0},
            'smoke': {'count': 0, 'active': False, 'last_alarm': 0}
        }
        self.alarm_normal_count = {  # 记录连续正常的次数
            'high_temp': 0,
            'low_temp': 0,
            'flame': 0,
            'smoke': 0
        }

    def connect(self):
        """连接到巴法云TCP云平台"""
        try:
            # 创建套接字
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)  # 设置超时
            self.socket.connect((BAFA_HOST, BAFA_PORT))
            print("已连接到巴法云TCP云平台")
            
            # 直接发送用户密钥进行认证
            auth_msg = f"{BAFA_KEY}\n".encode('utf-8')
            self.socket.sendall(auth_msg)
            print(f"已发送认证信息: {BAFA_KEY}")
            
            # 等待认证响应
            time.sleep(2.0)
            
            # 订阅TTS和STT主题
            encoded_uid = urllib.parse.quote(BAFA_KEY)
            subscribe_cmd = f"cmd=1&topic={BAFA_TOPIC_TTS_STT}&uid={encoded_uid}&type=3\n".encode('utf-8')
            self.socket.sendall(subscribe_cmd)
            print(f"已发送订阅请求: {BAFA_TOPIC_TTS_STT} (TTS/STT主题)")
            
            # 等待订阅响应
            time.sleep(1.0)
            
            # 订阅传感器数据和控制指令主题
            subscribe_cmd = f"cmd=1&topic={BAFA_TOPIC_SENSOR_CONTROL}&uid={encoded_uid}&type=3\n".encode('utf-8')
            self.socket.sendall(subscribe_cmd)
            print(f"已发送订阅请求: {BAFA_TOPIC_SENSOR_CONTROL} (传感器/控制主题)")
            
            # 等待订阅响应
            time.sleep(1.0)
            
            # 订阅预留主题
            subscribe_cmd = f"cmd=1&topic={BAFA_TOPIC_RESERVED}&uid={encoded_uid}&type=3\n".encode('utf-8')
            self.socket.sendall(subscribe_cmd)
            print(f"已发送订阅请求: {BAFA_TOPIC_RESERVED} (预留主题)")
            
            # 设置为已连接状态
            self.connected = True
            self.reconnect_count = 0
            self.last_heartbeat_time = time.time()
            print(f"成功连接并订阅所有主题")
            return True
            
        except Exception as e:
            print(f"连接失败: {str(e)}")
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
            return False

    def disconnect(self):
        """断开连接"""
        self.running = False
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.connected = False
        print("已断开连接")

    def send_message(self, message):
        """发送消息到巴法云平台"""
        if not self.connected or not self.socket:
            print("未连接，无法发送消息")
            return False

        try:
            # 添加到最近回复列表中
            self.recent_replies.append(message)
            # 保持最近回复列表不超过最大数量
            if len(self.recent_replies) > self.recent_replies_max_size:
                self.recent_replies.pop(0)
            
            # 构建标准格式的消息 - 完全按照终端代码格式
            # 对UID使用URL编码，但对消息内容不编码
            encoded_uid = urllib.parse.quote(BAFA_KEY)
            # 使用TTS/STT主题发送消息
            send_msg = f"cmd=2&topic={BAFA_TOPIC_TTS_STT}&msg={message}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
            
            # 打印发送信息（但不打印完整消息内容以避免输出过多）
            print(f"发送TTS消息到主题{BAFA_TOPIC_TTS_STT}，长度：{len(message)}字符，前20字符：{message[:20]}...")
            
            if self.socket:
                self.socket.sendall(send_msg)
                print("消息已发送")
                return True
            else:
                print("Socket未初始化，无法发送消息")
                return False
        except Exception as e:
            print(f"发送消息失败: {str(e)}")
            self.connected = False
            return False

    def send_heartbeat(self):
        """发送心跳包"""
        if not self.connected or not self.socket:
            return False

        try:
            # 使用与终端代码完全一致的心跳格式
            encoded_uid = urllib.parse.quote(BAFA_KEY)
            heartbeat_msg = f"cmd=0&uid={encoded_uid}&type=3\n".encode('utf-8')
            
            if self.socket:
                self.socket.sendall(heartbeat_msg)
                print("已发送心跳包")
                return True
            else:
                print("Socket未初始化，无法发送心跳包")
                return False
        except Exception as e:
            print(f"发送心跳包失败: {str(e)}")
            self.connected = False
            return False

    def reconnect(self):
        """重新连接"""
        if self.reconnect_count >= MAX_RECONNECT_ATTEMPTS:
            print(f"已达到最大重连次数({MAX_RECONNECT_ATTEMPTS})，停止重连")
            return False

        print(f"尝试重连，第{self.reconnect_count + 1}次...")
        self.reconnect_count += 1
        
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        
        time.sleep(RECONNECT_INTERVAL)
        return self.connect()

    def call_deepseek_api(self, message):
        """调用DeepSeek API进行对话"""
        try:
            # 如果消息包含URL编码，先解码
            if '%' in message:
                try:
                    decoded_message = urllib.parse.unquote(message)
                    print(f"已解码消息: {decoded_message}")
                    message = decoded_message
                except:
                    print("URL解码失败，使用原始消息")
            
            print(f"正在调用DeepSeek API，处理消息: {message}")
            
            # 添加用户消息到聊天历史
            self.chat_history.append({"role": "user", "content": message})
            
            # 保持历史记录在合理范围内，避免token超出限制
            if len(self.chat_history) > 10:
                # 保留系统提示和最近的9条消息
                system_messages = [msg for msg in self.chat_history if msg["role"] == "system"]
                other_messages = [msg for msg in self.chat_history if msg["role"] != "system"][-9:]
                self.chat_history = system_messages + other_messages
            
            headers = {
                "Content-Type": "application/json",
                "Authorization": f"Bearer {DEEPSEEK_API_KEY}"
            }
            
            data = {
                "model": "deepseek-chat",  # 使用DeepSeek-V3模型
                "messages": self.chat_history,
                "temperature": 0.7,
                "max_tokens": 300,  # 增加回复长度上限，因为我们会自己分段处理
                "stream": False
            }
            
            print("发送API请求...")
            response = requests.post(DEEPSEEK_API_URL, headers=headers, json=data)
            print(f"收到API响应，状态码: {response.status_code}")
            
            if response.status_code == 200:
                result = response.json()
                ai_reply = result['choices'][0]['message']['content']
                print(f"AI回复: {ai_reply[:50]}...")  # 只打印前50个字符
                
                # 将AI回复添加到聊天历史
                self.chat_history.append({"role": "assistant", "content": ai_reply})
                
                return ai_reply
            else:
                error_msg = f"API请求失败，状态码: {response.status_code}"
                print(error_msg)
                return f"很抱歉，无法回答您的问题。错误代码: {response.status_code}"
        except Exception as e:
            error_msg = f"调用DeepSeek API时发生错误: {str(e)}"
            print(error_msg)
            return "很抱歉，无法回答您的问题，请稍后再试。"

    def parse_segmented_message(self, message):
        """解析分段消息格式: prefix(m|n):content"""
        # 使用正则表达式匹配分段消息格式
        pattern = r'^([a-zA-Z]+)\((\d+)\|(\d+)\):(.*)$'
        match = re.match(pattern, message)
        
        if match:
            prefix = match.group(1)       # 消息前缀 (stt 或 tts)
            current_seg = int(match.group(2))  # 当前段号
            total_segs = int(match.group(3))   # 总段数
            content = match.group(4)     # 消息内容
            
            print(f"解析分段消息: 前缀={prefix}, 段={current_seg}/{total_segs}, 内容={content[:20]}...")
            return prefix, current_seg, total_segs, content
        else:
            return None, 0, 0, ""

    def process_segmented_message(self, message):
        """处理分段消息，返回是否是完整消息"""
        prefix, current_seg, total_segs, content = self.parse_segmented_message(message)
        
        if prefix is None:
            return False, None  # 不是分段消息
            
        # 如果是第一段，初始化分段消息存储
        if current_seg == 1:
            self.segmented_messages[prefix] = {
                "total_segments": total_segs,
                "segments": {},
                "complete": False
            }
            
        # 存储当前段
        if prefix in self.segmented_messages:
            self.segmented_messages[prefix]["segments"][current_seg] = content
            
            # 检查是否所有段都已接收
            if len(self.segmented_messages[prefix]["segments"]) == total_segs:
                # 按段号顺序合并所有段
                combined_content = ""
                for i in range(1, total_segs + 1):
                    if i in self.segmented_messages[prefix]["segments"]:
                        combined_content += self.segmented_messages[prefix]["segments"][i]
                        
                self.segmented_messages[prefix]["complete"] = True
                print(f"分段消息接收完成: 前缀={prefix}, 总段数={total_segs}, 合并后内容={combined_content[:30]}...")
                
                # 清理已处理的分段消息
                complete_message = combined_content
                del self.segmented_messages[prefix]
                
                return True, complete_message
                
        return False, None

    def segment_response(self, response):
        """将回复分段，每段最多MAX_SEGMENT_LENGTH个字符，并按照格式返回"""
        if not response:
            return []
            
        segments = []
        # 将长文本分段，每段最多MAX_SEGMENT_LENGTH个字符
        for i in range(0, len(response), MAX_SEGMENT_LENGTH):
            segments.append(response[i:i+MAX_SEGMENT_LENGTH])
            
        total_segments = len(segments)
        formatted_segments = []
        
        # 添加分段格式
        for i, segment in enumerate(segments):
            formatted_segment = f"{TTS_PREFIX}({i+1}|{total_segments}):{segment}"
            formatted_segments.append(formatted_segment)
            
        print(f"回复已分段: 总段数={total_segments}, 第一段={formatted_segments[0][:30]}...")
        return formatted_segments

    def process_received_line(self, line):
        """处理单行接收到的数据"""
        # 检查是否是心跳响应
        if "cmd=0&res=1" in line:
            print("收到心跳响应")
            return
        
        # 检查是否是发布确认响应
        if ("cmd=2&res=1" in line or "cmd=2&res=0" in line) and not "&msg=" in line:
            print("收到发布确认响应")
            return
            
        # 解析接收到的消息
        try:
            # 提取消息内容和主题
            msg_content = None
            topic = None
            
            # 提取主题
            if "&topic=" in line:
                topic_parts = line.split("&topic=")
                if len(topic_parts) > 1:
                    topic_part = topic_parts[1]
                    if "&" in topic_part:
                        topic = topic_part.split("&")[0]
                    else:
                        topic = topic_part
                        
                print(f"检测到消息主题: {topic}")
                
            # 消息格式检测
            if "&msg=" in line:
                print("检测到消息包含'&msg='，尝试提取消息内容")
                # 消息格式可能是 cmd=X&uid=xxx&topic=xxx&msg=xxx
                parts = line.split("&msg=")
                if len(parts) > 1:
                    # 有可能后面还有其他内容，如 cmd=2&res=1，需要清理
                    msg_part = parts[1]
                    if "\r\n" in msg_part:
                        msg_content = msg_part.split("\r\n")[0]
                    elif "cmd=" in msg_part:
                        msg_content = msg_part.split("cmd=")[0]
                    elif "&" in msg_part:  # 处理可能的额外参数
                        msg_content = msg_part.split("&")[0]
                    else:
                        msg_content = msg_part
                    
                    # 尝试URL解码
                    try:
                        msg_content = urllib.parse.unquote(msg_content)
                    except:
                        pass
                        
                    print(f"成功提取消息内容: {msg_content}")
                    
                    # 根据主题处理不同类型的消息
                    if topic == BAFA_TOPIC_TTS_STT:
                        # TTS/STT主题 - 处理语音识别结果
                        if msg_content and (
                            msg_content.startswith(f"{STT_PREFIX}:") or 
                            re.match(f"^{STT_PREFIX}\\(\\d+\\|\\d+\\):", msg_content)
                        ):
                            print(f"检测到STT消息: {msg_content}")
                            self.process_message(msg_content)
                        else:
                            print(f"收到TTS/STT主题的非STT消息，忽略处理: {msg_content[:30]}...")
                    
                    elif topic == BAFA_TOPIC_SENSOR_CONTROL:
                        # 传感器/控制主题 - 处理传感器数据和控制指令反馈
                        # 检查是否是传感器数据
                        if "ADC0=" in msg_content and "温度=" in msg_content:
                            print(f"检测到传感器数据: {msg_content[:30]}...")
                            self.process_sensor_data(msg_content)
                            
                        # 检查是否是到达通知
                        if msg_content.startswith("ARRIVAL_NOTIFICATION:"):
                            arrival_info = msg_content[len("ARRIVAL_NOTIFICATION:"):]
                            print(f"收到小车到达通知: {arrival_info}")
                            
                            # 如果最后一个命令是导航命令，更新其状态
                            if self.last_command and self.last_command['command'].startswith("NAVIGATE="):
                                self.last_command['status'] = 'completed'
                                self.last_command['complete_time'] = time.time()
                                print(f"导航指令 {self.last_command['command']} 已完成执行")
                                
                                # 添加到历史记录
                                self.command_history.append(self.last_command.copy())
                                
                                # 发送TTS消息告知用户小车已到达目的地
                                location = self.last_command.get('location', '目的地')
                                tts_message = f"tts:我已经到达{location}，正在执行任务。"
                                
                                encoded_uid = urllib.parse.quote(BAFA_KEY)
                                tts_cmd = f"cmd=2&topic={BAFA_TOPIC_TTS_STT}&msg={tts_message}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                                
                                if self.socket:
                                    self.socket.sendall(tts_cmd)
                                    print(f"已发送到达TTS消息: {tts_message}")
                            
                        # 检查是否是控制指令的反馈
                        elif self.last_command and (
                            "LED=ON" in msg_content or "LED=OFF" in msg_content or "LED=BLINK" in msg_content or
                            "BUZZER=ON" in msg_content or "BUZZER=OFF" in msg_content or "BUZZER=BEEP" in msg_content or "BUZZER=SOS" in msg_content or
                            "MODE=AUTO" in msg_content or "MODE=MANUAL" in msg_content
                        ):
                            # 更新最后发送的命令状态
                            if self.last_command['status'] == 'sent':
                                self.last_command['status'] = 'confirmed'
                                self.last_command['confirm_time'] = time.time()
                                print(f"控制指令 {self.last_command['command']} 已被确认执行")
                                
                                # 添加到历史记录
                                self.command_history.append(self.last_command.copy())
                    
                    elif topic == BAFA_TOPIC_RESERVED:
                        # 预留主题 - 目前不做处理
                        print(f"收到预留主题的消息: {msg_content[:30]}...")
                    
                    else:
                        print(f"收到未知主题的消息: {msg_content[:30]}...")
            else:
                # 只在消息不为空时打印
                if line.strip():
                    print(f"消息中没有找到msg字段: {line}")
        except Exception as e:
            print(f"解析消息时出错: {str(e)}")
            print(f"原始消息: {line}")
            
    def process_message(self, message):
        """处理接收到的消息，调用AI，并发送回复"""
        print("=== 开始处理新消息 ===")
        print(f"收到消息: {message}")
        
        # 检查是否是我们自己发送的消息（防止回环）
        if any(message in reply for reply in self.recent_replies):
            print("检测到回环：这是我们自己发送的消息，忽略处理")
            print("=== 消息处理完成（已忽略） ===")
            return
        
        # 检查消息是否符合要求的格式(stt:或stt(m|n):)
        is_valid_format = False
        user_message = ""
        
        # 检查是否是分段消息
        is_segmented, complete_message = self.process_segmented_message(message)
        
        if is_segmented:
            if complete_message is None:
                # 分段消息尚未完成，等待更多段
                print("分段消息尚未接收完全，等待更多段...")
                return
            else:
                # 分段消息接收完成，使用合并后的消息
                user_message = complete_message
                print(f"使用合并后的完整消息: {user_message[:30]}...")
                is_valid_format = True
        # 检查是否是简单的stt:前缀
        elif message.startswith(f"{STT_PREFIX}:"):
            user_message = message[len(STT_PREFIX)+1:]
            print(f"检测到STT前缀，去除前缀后的消息: {user_message[:30]}...")
            is_valid_format = True
            
        # 如果不是有效的格式，不处理
        if not is_valid_format:
            print(f"消息格式不符合要求(stt:或stt(m|n):)，忽略处理: {message[:30]}...")
            print("=== 消息处理完成（已忽略） ===")
            return
            
        # 首先调用控制指令AI识别可能的控制指令
        control_cmds = self.call_control_ai(user_message)
        
        # 如果识别出控制指令，发送到巴法云
        control_executed = False
        executed_cmds = []
        
        if control_cmds:
            print(f"识别出控制指令: {control_cmds}")
            
            # 检查是否包含特殊命令（FIND或HELP）
            has_special_cmd = False
            special_cmds = []
            normal_cmds = []
            
            for cmd in control_cmds:
                if cmd == 'FIND=ON' or cmd == 'HELP=ON':
                    has_special_cmd = True
                    special_cmds.append(cmd)
                else:
                    normal_cmds.append(cmd)
            
            # 如果有特殊命令，优先处理
            if has_special_cmd:
                print(f"检测到特殊命令: {special_cmds}")
                if self.send_control_command(special_cmds):
                    control_executed = True
                    executed_cmds.extend(special_cmds)
                    print(f"特殊命令已发送")
                    
                    # 特殊命令会自动触发LED和蜂鸣器，不需要再发送这些命令
                    filtered_normal_cmds = [cmd for cmd in normal_cmds if not cmd.startswith('LED=') and not cmd.startswith('BUZZER=')]
                    if filtered_normal_cmds:
                        if self.send_control_command(filtered_normal_cmds):
                            executed_cmds.extend(filtered_normal_cmds)
                            print(f"其他普通命令已发送")
                else:
                    print(f"特殊命令发送失败")
            else:
                # 处理普通命令
                if self.send_control_command(normal_cmds):
                    control_executed = True
                    executed_cmds = normal_cmds
                    print(f"控制指令已全部发送")
                else:
                    print(f"部分或全部控制指令发送失败")
        else:
            print("未识别出控制指令")
            
        # 构建发送给对话AI的消息，包含控制信息
        control_info = "null"
        if control_executed and executed_cmds:
            control_info = "|".join(executed_cmds) if isinstance(executed_cmds, list) else executed_cmds
        
        ai_input = f"控制:{control_info},原文本:{user_message}"
        print(f"发送给对话AI的消息: {ai_input}")
        
        # 调用DeepSeek API获取回复
        ai_reply = self.call_deepseek_api(ai_input)
        
        # 将回复分段
        formatted_segments = self.segment_response(ai_reply)
        
        # 分段发送回复
        sent_all = True
        for segment in formatted_segments:
            success = self.send_message(segment)
            if not success:
                sent_all = False
                print(f"发送段 {segment[:30]}... 失败")
                break
            else:
                print(f"成功发送段: {segment[:30]}...")
            # 增加延迟，避免发送过快导致消息丢失
            time.sleep(SEGMENT_DELAY)
            
        if sent_all:
            print("所有AI回复段已成功发送到巴法云")
        else:
            print("部分AI回复段发送到巴法云失败")
        
        print("=== 消息处理完成 ===")

    def process_sensor_data(self, data):
        """处理并存储ESP32上传的传感器数据"""
        try:
            print("处理传感器数据...")
            # 解析数据
            sensor_info = {}
            
            # 提取温度和湿度
            temp_match = re.search(r'温度=([0-9.]+)C', data)
            humi_match = re.search(r'湿度=([0-9.]+)%', data)
            if temp_match:
                sensor_info['temperature'] = float(temp_match.group(1))
            if humi_match:
                sensor_info['humidity'] = float(humi_match.group(1))
            
            # 提取ADC值
            adc0_match = re.search(r'ADC0=(\d+)', data)  # 火焰传感器
            adc4_match = re.search(r'ADC4=(\d+)', data)  # 烟雾传感器
            adc7_match = re.search(r'ADC7=(\d+)', data)  # 光线传感器
            if adc0_match:
                sensor_info['flame_sensor'] = int(adc0_match.group(1))
            if adc4_match:
                sensor_info['smoke_sensor'] = int(adc4_match.group(1))
            if adc7_match:
                sensor_info['light_sensor'] = int(adc7_match.group(1))
            
            # 提取LED和蜂鸣器状态
            led_match = re.search(r'LED=(ON|OFF)', data)
            buzzer_match = re.search(r'BUZZER=(ON|OFF)', data)
            led_mode_match = re.search(r'LED_MODE=(\w+)', data)
            buzzer_mode_match = re.search(r'BUZZER_MODE=(\w+)', data)
            auto_match = re.search(r'AUTO=(ON|OFF)', data)
            
            if led_match:
                sensor_info['led'] = led_match.group(1)
            if buzzer_match:
                sensor_info['buzzer'] = buzzer_match.group(1)
            if led_mode_match:
                sensor_info['led_mode'] = led_mode_match.group(1)
            if buzzer_mode_match:
                sensor_info['buzzer_mode'] = buzzer_mode_match.group(1)
            if auto_match:
                sensor_info['auto'] = auto_match.group(1)
            
            # 添加时间戳
            sensor_info['timestamp'] = time.time()
            
            # 添加到队列
            self.sensor_data_queue.append(sensor_info)
            print(f"已存储传感器数据，当前队列长度: {len(self.sensor_data_queue)}")
            print(f"最新数据: {sensor_info}")
            
            # 检查是否需要触发报警
            self.check_alarm_conditions(sensor_info)
            
        except Exception as e:
            print(f"处理传感器数据时出错: {str(e)}")
            
    def check_alarm_conditions(self, sensor_data):
        """检查是否需要触发报警"""
        current_time = time.time()
        alarms_triggered = []
        
        # 检查温度过高
        if 'temperature' in sensor_data:
            temp = sensor_data['temperature']
            if temp > TEMP_HIGH_THRESHOLD:
                self.alarm_status['high_temp']['count'] += 1
                self.alarm_normal_count['high_temp'] = 0
                
                if (self.alarm_status['high_temp']['count'] >= ALARM_CONSECUTIVE_COUNT and 
                    not self.alarm_status['high_temp']['active']):
                    # 触发温度过高报警
                    self.alarm_status['high_temp']['active'] = True
                    self.alarm_status['high_temp']['last_alarm'] = current_time
                    alarms_triggered.append(('high_temp', temp))
                    print(f"触发温度过高报警: {temp}°C")
            elif temp <= TEMP_HIGH_THRESHOLD:
                self.alarm_normal_count['high_temp'] += 1
                if (self.alarm_normal_count['high_temp'] >= ALARM_CONSECUTIVE_COUNT and 
                    self.alarm_status['high_temp']['active']):
                    # 解除温度过高报警
                    self.alarm_status['high_temp']['active'] = False
                    self.alarm_status['high_temp']['count'] = 0
                    print(f"解除温度过高报警: {temp}°C")
                    
            # 检查温度过低
            if temp < TEMP_LOW_THRESHOLD:
                self.alarm_status['low_temp']['count'] += 1
                self.alarm_normal_count['low_temp'] = 0
                
                if (self.alarm_status['low_temp']['count'] >= ALARM_CONSECUTIVE_COUNT and 
                    not self.alarm_status['low_temp']['active']):
                    # 触发温度过低报警
                    self.alarm_status['low_temp']['active'] = True
                    self.alarm_status['low_temp']['last_alarm'] = current_time
                    alarms_triggered.append(('low_temp', temp))
                    print(f"触发温度过低报警: {temp}°C")
            elif temp >= TEMP_LOW_THRESHOLD:
                self.alarm_normal_count['low_temp'] += 1
                if (self.alarm_normal_count['low_temp'] >= ALARM_CONSECUTIVE_COUNT and 
                    self.alarm_status['low_temp']['active']):
                    # 解除温度过低报警
                    self.alarm_status['low_temp']['active'] = False
                    self.alarm_status['low_temp']['count'] = 0
                    print(f"解除温度过低报警: {temp}°C")
        
        # 检查火焰传感器
        if 'flame_sensor' in sensor_data:
            flame_value = sensor_data['flame_sensor']
            if flame_value < FLAME_THRESHOLD:  # 值越小越危险
                self.alarm_status['flame']['count'] += 1
                self.alarm_normal_count['flame'] = 0
                
                if (self.alarm_status['flame']['count'] >= ALARM_CONSECUTIVE_COUNT and 
                    not self.alarm_status['flame']['active']):
                    # 触发火焰报警
                    self.alarm_status['flame']['active'] = True
                    self.alarm_status['flame']['last_alarm'] = current_time
                    alarms_triggered.append(('flame', flame_value))
                    print(f"触发火焰报警: 值={flame_value}")
                    
                    # 火焰报警是严重的安全问题，立即触发LED闪烁和蜂鸣器长鸣
                    self.send_control_command(['LED=BLINK', 'BUZZER=ON'])
            elif flame_value >= FLAME_THRESHOLD:
                self.alarm_normal_count['flame'] += 1
                if (self.alarm_normal_count['flame'] >= ALARM_CONSECUTIVE_COUNT and 
                    self.alarm_status['flame']['active']):
                    # 解除火焰报警
                    self.alarm_status['flame']['active'] = False
                    self.alarm_status['flame']['count'] = 0
                    print(f"解除火焰报警: 值={flame_value}")
                    
                    # 如果没有其他严重报警，关闭LED闪烁和蜂鸣器
                    if not self.alarm_status['smoke']['active']:
                        self.send_control_command(['LED=OFF', 'BUZZER=OFF'])
        
        # 检查烟雾传感器
        if 'smoke_sensor' in sensor_data:
            smoke_value = sensor_data['smoke_sensor']
            if smoke_value < SMOKE_THRESHOLD:  # 值越小越危险
                self.alarm_status['smoke']['count'] += 1
                self.alarm_normal_count['smoke'] = 0
                
                if (self.alarm_status['smoke']['count'] >= ALARM_CONSECUTIVE_COUNT and 
                    not self.alarm_status['smoke']['active']):
                    # 触发烟雾报警
                    self.alarm_status['smoke']['active'] = True
                    self.alarm_status['smoke']['last_alarm'] = current_time
                    alarms_triggered.append(('smoke', smoke_value))
                    print(f"触发烟雾报警: 值={smoke_value}")
                    
                    # 烟雾报警是严重的安全问题，立即触发LED闪烁和蜂鸣器长鸣
                    self.send_control_command(['LED=BLINK', 'BUZZER=ON'])
            elif smoke_value >= SMOKE_THRESHOLD:
                self.alarm_normal_count['smoke'] += 1
                if (self.alarm_normal_count['smoke'] >= ALARM_CONSECUTIVE_COUNT and 
                    self.alarm_status['smoke']['active']):
                    # 解除烟雾报警
                    self.alarm_status['smoke']['active'] = False
                    self.alarm_status['smoke']['count'] = 0
                    print(f"解除烟雾报警: 值={smoke_value}")
                    
                    # 如果没有其他严重报警，关闭LED闪烁和蜂鸣器
                    if not self.alarm_status['flame']['active']:
                        self.send_control_command(['LED=OFF', 'BUZZER=OFF'])
        
        # 如果有报警被触发，发送报警消息
        if alarms_triggered:
            self.send_alarm_notification(alarms_triggered)
            
    def send_alarm_notification(self, alarms):
        """发送报警通知"""
        # 检查是否可以发送报警（冷却时间）
        current_time = time.time()
        
        for alarm_type, value in alarms:
            # 检查报警冷却时间
            last_alarm_time = self.alarm_status[alarm_type]['last_alarm']
            if current_time - last_alarm_time < ALARM_COOLDOWN:
                print(f"{alarm_type}报警在冷却期内，跳过通知")
                continue
                
            # 更新最后报警时间
            self.alarm_status[alarm_type]['last_alarm'] = current_time
            
            # 构建报警消息
            alarm_message = ""
            if alarm_type == 'high_temp':
                alarm_message = f"报警:温度过高报警,原文本:检测到温度异常升高，当前温度{value}°C，已超过{TEMP_HIGH_THRESHOLD}°C的安全阈值。"
            elif alarm_type == 'low_temp':
                alarm_message = f"报警:温度过低报警,原文本:检测到温度异常降低，当前温度{value}°C，已低于{TEMP_LOW_THRESHOLD}°C的安全阈值。"
            elif alarm_type == 'flame':
                alarm_message = f"报警:火焰报警,原文本:检测到火焰信号，这可能表明有火灾风险，请立即检查！"
            elif alarm_type == 'smoke':
                alarm_message = f"报警:烟雾报警,原文本:检测到烟雾信号，这可能表明有火灾或气体泄漏风险，请立即检查！"
            
            if alarm_message:
                # 调用DeepSeek API获取回复
                ai_reply = self.call_deepseek_api(alarm_message)
                
                # 将回复分段
                formatted_segments = self.segment_response(ai_reply)
                
                # 分段发送回复
                for segment in formatted_segments:
                    success = self.send_message(segment)
                    if not success:
                        print(f"发送报警通知段 {segment[:30]}... 失败")
                        break
                    # 增加延迟，避免发送过快导致消息丢失
                    time.sleep(SEGMENT_DELAY)

    def call_control_ai(self, message):
        """调用控制指令AI，识别用户语言中的控制指令"""
        try:
            print(f"正在调用控制指令AI，分析消息: {message}")
            
            # 初始化控制AI的对话历史
            if not self.control_chat_history:
                self.control_chat_history = [{"role": "system", "content": CONTROL_AI_PROMPT}]
            
            # 添加用户消息到控制AI的对话历史
            self.control_chat_history.append({"role": "user", "content": message})
            
            # 保持历史记录在合理范围内
            if len(self.control_chat_history) > 5:
                # 保留系统提示和最近的4条消息
                system_messages = [msg for msg in self.control_chat_history if msg["role"] == "system"]
                other_messages = [msg for msg in self.control_chat_history if msg["role"] != "system"][-4:]
                self.control_chat_history = system_messages + other_messages
            
            headers = {
                "Content-Type": "application/json",
                "Authorization": f"Bearer {DEEPSEEK_API_KEY}"
            }
            
            data = {
                "model": "deepseek-chat",
                "messages": self.control_chat_history,
                "temperature": 0.3,  # 使用较低的温度以获得更确定的回复
                "max_tokens": 50,     # 控制指令回复较短
                "stream": False
            }
            
            print("发送控制AI请求...")
            response = requests.post(DEEPSEEK_API_URL, headers=headers, json=data)
            print(f"收到控制AI响应，状态码: {response.status_code}")
            
            if response.status_code == 200:
                result = response.json()
                ai_reply = result['choices'][0]['message']['content']
                print(f"控制AI回复: {ai_reply}")
                
                # 将AI回复添加到控制AI的对话历史
                self.control_chat_history.append({"role": "assistant", "content": ai_reply})
                
                # 提取控制指令
                control_cmds = self.extract_control_command(ai_reply)
                return control_cmds
            else:
                error_msg = f"控制AI请求失败，状态码: {response.status_code}"
                print(error_msg)
                return None
        except Exception as e:
            error_msg = f"调用控制指令AI时发生错误: {str(e)}"
            print(error_msg)
            return None
    
    def extract_control_command(self, ai_reply):
        """从AI回复中提取控制指令"""
        # 使用正则表达式提取控制指令，支持所有命令类型
        match = re.search(r'控制指令:((?:LED=\w+|BUZZER=\w+|MODE=\w+|NAVIGATE=[\w\u4e00-\u9fa5]+|FIND=\w+|HELP=\w+)(?:\|(?:LED=\w+|BUZZER=\w+|MODE=\w+|NAVIGATE=[\w\u4e00-\u9fa5]+|FIND=\w+|HELP=\w+))*|null)', ai_reply)
        if match:
            cmd = match.group(1)
            if cmd == "null":
                return None
            
            # 如果有多个命令（用|分隔），返回命令列表
            if "|" in cmd:
                return cmd.split("|")
            
            # 单个命令直接返回
            return [cmd]
        return None
    
    def send_control_command(self, command):
        """发送控制指令到巴法云平台"""
        if not command:
            print("没有控制指令需要发送")
            return False
            
        if not self.connected or not self.socket:
            print("未连接，无法发送控制指令")
            return False
        
        # 如果是命令列表，则逐个发送
        if isinstance(command, list):
            all_success = True
            for cmd in command:
                success = self._send_single_command(cmd)
                if not success:
                    all_success = False
            return all_success
        else:
            # 单个命令
            return self._send_single_command(command)
        
    def _send_single_command(self, command):
        """发送单个控制指令"""
        try:
            # 检查是否是导航命令
            navigate_match = re.match(r'^NAVIGATE=([\w\u4e00-\u9fa5]+)$', command)
            if navigate_match:
                # 提取位置名称
                location = navigate_match.group(1)
                print(f"检测到导航命令，目标位置: {location}")
                
                # 构建导航指令消息，发送到小车控制主题(text3)
                encoded_uid = urllib.parse.quote(BAFA_KEY)
                # 发送TTS消息告知用户小车即将前往目的地
                tts_message = f"tts:好的，我现在去{location}"
                tts_cmd = f"cmd=2&topic={BAFA_TOPIC_TTS_STT}&msg={tts_message}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                
                # 发送导航命令到text3主题
                navigate_cmd = f"cmd=2&topic={BAFA_TOPIC_RESERVED}&msg=AREA_NAVIGATE:{location}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                
                print(f"发送TTS消息到主题{BAFA_TOPIC_TTS_STT}: {tts_message}")
                print(f"发送导航指令到主题{BAFA_TOPIC_RESERVED}: AREA_NAVIGATE:{location}")
                
                if self.socket:
                    # 先发送TTS消息
                    self.socket.sendall(tts_cmd)
                    time.sleep(0.5)
                    # 再发送导航指令
                    self.socket.sendall(navigate_cmd)
                    
                    # 记录最后发送的命令和时间
                    self.last_command = {
                        'command': command,
                        'time': time.time(),
                        'status': 'sent',
                        'location': location
                    }
                    return True
                else:
                    print("Socket未初始化，无法发送导航指令")
                    return False
            # 检查是否是寻找小车命令
            elif command == 'FIND=ON':
                print("检测到寻找小车命令")
                
                # 寻找小车模式：LED闪烁 + 蜂鸣器间歇鸣叫
                encoded_uid = urllib.parse.quote(BAFA_KEY)
                # 发送TTS消息告知用户
                tts_message = f"tts:我正在闪烁灯光并发出声音，请查看周围环境"
                tts_cmd = f"cmd=2&topic={BAFA_TOPIC_TTS_STT}&msg={tts_message}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                
                # 发送控制命令
                find_cmds = ['LED=BLINK', 'BUZZER=BEEP']
                
                if self.socket:
                    # 先发送TTS消息
                    self.socket.sendall(tts_cmd)
                    time.sleep(0.5)
                    
                    # 发送控制命令
                    for cmd in find_cmds:
                        control_msg = f"cmd=2&topic={BAFA_TOPIC_SENSOR_CONTROL}&msg={cmd}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                        self.socket.sendall(control_msg)
                        time.sleep(0.5)
                    
                    # 记录最后发送的命令和时间
                    self.last_command = {
                        'command': command,
                        'time': time.time(),
                        'status': 'sent',
                        'sub_commands': find_cmds
                    }
                    return True
                else:
                    print("Socket未初始化，无法发送寻找小车命令")
                    return False
            # 检查是否是求救命令
            elif command == 'HELP=ON':
                print("检测到求救命令")
                
                # 求救模式：LED闪烁 + 蜂鸣器SOS模式
                encoded_uid = urllib.parse.quote(BAFA_KEY)
                # 发送TTS消息告知用户
                tts_message = f"tts:我已经激活了紧急求救模式，正在发出SOS信号"
                tts_cmd = f"cmd=2&topic={BAFA_TOPIC_TTS_STT}&msg={tts_message}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                
                # 发送控制命令
                help_cmds = ['LED=BLINK', 'BUZZER=SOS']
                
                if self.socket:
                    # 先发送TTS消息
                    self.socket.sendall(tts_cmd)
                    time.sleep(0.5)
                    
                    # 发送控制命令
                    for cmd in help_cmds:
                        control_msg = f"cmd=2&topic={BAFA_TOPIC_SENSOR_CONTROL}&msg={cmd}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                        self.socket.sendall(control_msg)
                        time.sleep(0.5)
                    
                    # 记录最后发送的命令和时间
                    self.last_command = {
                        'command': command,
                        'time': time.time(),
                        'status': 'sent',
                        'sub_commands': help_cmds
                    }
                    return True
                else:
                    print("Socket未初始化，无法发送求救命令")
                    return False
            else:
                # 验证标准控制命令格式
                if not re.match(r'^(LED=ON|LED=OFF|LED=BLINK|BUZZER=ON|BUZZER=OFF|BUZZER=BEEP|BUZZER=SOS|MODE=AUTO|MODE=MANUAL)$', command):
                    print(f"警告：无效的控制指令格式: {command}")
                    return False
                    
                # 构建控制指令消息
                encoded_uid = urllib.parse.quote(BAFA_KEY)
                # 使用传感器/控制主题发送控制指令
                control_msg = f"cmd=2&topic={BAFA_TOPIC_SENSOR_CONTROL}&msg={command}&uid={encoded_uid}&from={encoded_uid}\n".encode('utf-8')
                
                print(f"发送控制指令到主题{BAFA_TOPIC_SENSOR_CONTROL}: {command}")
                if self.socket:
                    self.socket.sendall(control_msg)
                    print(f"控制指令 {command} 已发送")
                    
                    # 在发送多个命令时添加短暂延迟，避免命令堆积
                    time.sleep(0.5)
                    
                    # 记录最后发送的命令和时间
                    self.last_command = {
                        'command': command,
                        'time': time.time(),
                        'status': 'sent'
                    }
                    return True
                else:
                    print("Socket未初始化，无法发送控制指令")
                    return False
        except Exception as e:
            print(f"发送控制指令失败: {str(e)}")
            self.connected = False
            return False

    def get_status_report(self):
        """获取系统状态报告"""
        report = {
            "connected": self.connected,
            "reconnect_count": self.reconnect_count,
            "last_heartbeat": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(self.last_heartbeat_time)),
            "sensor_data_count": len(self.sensor_data_queue),
            "latest_sensor_data": self.sensor_data_queue[-1] if self.sensor_data_queue else None,
            "last_command": self.last_command,
            "command_history_count": len(self.command_history)
        }
        
        return report
        
    def print_status(self):
        """打印系统状态"""
        status = self.get_status_report()
        print("\n=== 系统状态报告 ===")
        print(f"连接状态: {'已连接' if status['connected'] else '未连接'}")
        print(f"重连次数: {status['reconnect_count']}")
        print(f"最后心跳时间: {status['last_heartbeat']}")
        print(f"传感器数据数量: {status['sensor_data_count']}")
        if status['latest_sensor_data']:
            print(f"最新传感器数据: {status['latest_sensor_data']}")
        if status['last_command']:
            print(f"最后控制指令: {status['last_command']['command']}")
            print(f"指令状态: {status['last_command']['status']}")
            print(f"发送时间: {time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(status['last_command']['time']))}")
        print(f"历史指令数量: {status['command_history_count']}")
        print("=== 状态报告结束 ===\n")
        
    def run(self):
        """运行主循环，处理接收消息和心跳"""
        self.running = True
        
        # 初始化聊天历史，添加自定义提示词
        self.chat_history = [{"role": "system", "content": AI_PROMPT}]
        
        if not self.connect():
            print("初始连接失败，尝试重连...")
            if not self.reconnect():
                print("重连失败，程序退出")
                return

        # 创建接收线程
        receive_thread = threading.Thread(target=self.receive_loop)
        receive_thread.daemon = True
        receive_thread.start()

        # 创建状态监控线程
        status_thread = threading.Thread(target=self.status_monitor)
        status_thread.daemon = True
        status_thread.start()

        # 主线程处理心跳
        try:
            while self.running:
                # 检查心跳
                if time.time() - self.last_heartbeat_time >= HEARTBEAT_INTERVAL:
                    if not self.send_heartbeat():
                        print("心跳失败，尝试重连...")
                        if not self.reconnect():
                            print("重连失败，退出程序")
                            break
                    self.last_heartbeat_time = time.time()
                
                time.sleep(1)  # 减少CPU使用率
        except KeyboardInterrupt:
            print("程序被用户中断")
        finally:
            self.disconnect()
            
    def status_monitor(self):
        """状态监控线程，定期打印系统状态"""
        while self.running:
            # 每60秒打印一次状态
            self.print_status()
            time.sleep(60)
            
            # 检查最后发送的命令是否超时
            if self.last_command and self.last_command['status'] == 'sent' and time.time() - self.last_command['time'] > 10:
                print(f"警告：控制指令 {self.last_command['command']} 可能未被执行，已超过10秒未收到确认")
                self.last_command['status'] = 'timeout'
                # 添加到历史记录
                self.command_history.append(self.last_command.copy())

    def receive_loop(self):
        """接收消息循环"""
        buffer = ""  # 用于处理不完整的消息
        while self.running:
            if not self.connected or not self.socket:
                time.sleep(1)
                continue
                
            try:
                if self.socket:
                    data = self.socket.recv(4096)
                    if not data:  # 连接已关闭
                        print("连接已关闭，尝试重连...")
                        self.connected = False
                        if not self.reconnect():
                            print("重连失败，退出接收循环")
                            break
                        continue
                    
                    # 处理接收到的数据
                    try:
                        message = data.decode('utf-8')
                        
                        # 不打印空数据
                        if not message.strip():
                            continue
                            
                        print(f"\n收到原始数据: {message}")
                        
                        # 将新数据添加到缓冲区
                        buffer += message
                        
                        # 处理可能的多行消息
                        while "\n" in buffer:
                            line, buffer = buffer.split("\n", 1)
                            if line.strip():  # 只处理非空行
                                self.process_received_line(line.strip())
                    except UnicodeDecodeError:
                        print("收到无法解码的数据，忽略")
                else:
                    print("Socket未初始化，等待重连...")
                    time.sleep(1)
                    
            except socket.timeout:
                continue
            except Exception as e:
                print(f"接收消息时出错: {str(e)}")
                self.connected = False
                if not self.reconnect():
                    print("重连失败，退出接收循环")
                    break
                    
if __name__ == "__main__":
    client = BafaCloudClient()
    try:
        print("巴法云 + DeepSeek AI 对话系统启动")
        print(f"已配置主题: TTS/STT={BAFA_TOPIC_TTS_STT}, 传感器/控制={BAFA_TOPIC_SENSOR_CONTROL}, 预留={BAFA_TOPIC_RESERVED}")
        print(f"已加载AI提示词: \n{AI_PROMPT}")
        print(f"已加载控制AI提示词: \n{CONTROL_AI_PROMPT}")
        print(f"分段消息配置: 每段最大{MAX_SEGMENT_LENGTH}字符, STT前缀={STT_PREFIX}, TTS前缀={TTS_PREFIX}, 发送间隔={SEGMENT_DELAY}秒")
        
        # 运行主客户端
        client.run()
    except KeyboardInterrupt:
        print("程序被用户中断")
        client.disconnect() 