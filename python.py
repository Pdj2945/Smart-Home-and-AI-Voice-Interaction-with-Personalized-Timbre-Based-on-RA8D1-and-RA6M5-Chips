import tkinter as tk
from tkinter import messagebox, simpledialog, ttk
import socket
import threading
import time
import math
import json
from urllib import parse
import os


class AStar:
    def __init__(self, grid):
        self.grid = grid
        self.rows = len(grid)
        self.cols = len(grid[0]) if self.rows > 0 else 0
        self.directions = [(0, 1), (1, 0), (0, -1), (-1, 0)]  # 右, 下, 左, 上
        # 墙体安全距离（格数）
        self.wall_safety_distance = 2

    def heuristic(self, a, b):
        # 使用曼哈顿距离作为启发式函数
        return abs(a[0] - b[0]) + abs(a[1] - b[1])

    def is_valid(self, pos):
        # 检查位置是否在网格范围内且不是障碍物
        return 0 <= pos[0] < self.rows and 0 <= pos[1] < self.cols and self.grid[pos[0]][pos[1]] == 0

    def calculate_safety_score(self, pos):
        """计算位置的安全分数，距离墙体越远分数越高"""
        row, col = pos
        min_wall_distance = float('inf')
        
        # 检查周围的墙体距离
        for r in range(max(0, row - self.wall_safety_distance), min(self.rows, row + self.wall_safety_distance + 1)):
            for c in range(max(0, col - self.wall_safety_distance), min(self.cols, col + self.wall_safety_distance + 1)):
                if self.grid[r][c] == 1:  # 如果是墙体
                    distance = max(abs(r - row), abs(c - col))
                    min_wall_distance = min(min_wall_distance, distance)
        
        if min_wall_distance == float('inf'):
            # 附近没有墙体，安全系数最高
            return 0
        elif min_wall_distance < self.wall_safety_distance:
            # 距离墙体较近，增加成本
            return (self.wall_safety_distance - min_wall_distance) * 2
        else:
            # 距离墙体足够远，无额外成本
            return 0

    def is_narrow_passage(self, pos):
        """检查是否是狭窄通道（宽度小于安全距离）"""
        row, col = pos
        horizontal_walls = 0
        vertical_walls = 0
        
        # 检查水平方向
        for c in range(max(0, col - self.wall_safety_distance), min(self.cols, col + self.wall_safety_distance + 1)):
            if 0 <= row - self.wall_safety_distance < self.rows and self.grid[row - self.wall_safety_distance][c] == 1:
                horizontal_walls += 1
            if 0 <= row + self.wall_safety_distance < self.rows and self.grid[row + self.wall_safety_distance][c] == 1:
                horizontal_walls += 1
                
        # 检查垂直方向
        for r in range(max(0, row - self.wall_safety_distance), min(self.rows, row + self.wall_safety_distance + 1)):
            if 0 <= col - self.wall_safety_distance < self.cols and self.grid[r][col - self.wall_safety_distance] == 1:
                vertical_walls += 1
            if 0 <= col + self.wall_safety_distance < self.cols and self.grid[r][col + self.wall_safety_distance] == 1:
                vertical_walls += 1
        
        # 如果任一方向的墙体数量超过阈值，认为是狭窄通道
        return horizontal_walls > self.wall_safety_distance or vertical_walls > self.wall_safety_distance

    def get_path(self, start, goal):
        open_set = [(0, start)]
        came_from = {}
        g_score = {start: 0}
        f_score = {start: self.heuristic(start, goal)}

        while open_set:
            open_set.sort()
            _, current = open_set.pop(0)

            if current == goal:
                path = []
                while current in came_from:
                    path.append(current)
                    current = came_from[current]
                path.append(start)
                return path[::-1]

            # 只使用四个主方向
            for dx, dy in self.directions:
                neighbor = (current[0] + dx, current[1] + dy)
                if not self.is_valid(neighbor):
                    continue

                # 计算移动成本：基础成本 + 安全分数
                move_cost = 1
                
                # 如果不是狭窄通道，添加安全距离成本
                if not self.is_narrow_passage(neighbor):
                    safety_score = self.calculate_safety_score(neighbor)
                else:
                    # 狭窄通道不考虑安全距离
                    safety_score = 0
                    
                tentative_g = int(g_score[current] + move_cost + safety_score)

                if neighbor not in g_score or tentative_g < g_score[neighbor]:
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g
                    f_score[neighbor] = tentative_g + self.heuristic(neighbor, goal)
                    if (f_score[neighbor], neighbor) not in open_set:
                        open_set.append((f_score[neighbor], neighbor))

        return None  # 没有找到路径


class BFYunClient:
    # 巴法云TCP配置（根据控制台信息补充）
    BEMFA_CONFIG = {
        "host": "bemfa.com",
        "port": 8344,
        "uid": "11ca31c562d14ef786a0238a03a3be16",  # 控制台显示的私钥
        "type": 3,  # TCP协议类型
        "topics": {"navigation": "text3"},  # 使用text3作为小车控制主题
        "heartbeat_interval": 50  # 心跳间隔(秒)
    }

    def __init__(self, topic="text3"):
        self.topic = topic  # 使用text3主题与小车对应
        self.tts_topic = "text3"  # TTS消息也使用text3主题，确保小车能接收到
        self.socket = None
        self.connected = False
        self.receive_thread = None
        self.heartbeat_thread = None
        self.stop_event = threading.Event()
        self.current_position = None  # 当前位置 (x, y, dir)
        self.previous_position = None  # 上一位置
        self.callback = None  # 位置更新回调
        self.waiting_for_position = False  # 是否正在等待位置反馈
        self.connection_lock = threading.Lock()  # 添加锁以保护socket操作

    def connect(self):
        """建立TCP连接并完成认证"""
        try:
            # 关闭现有连接
            with self.connection_lock:
                if self.socket:
                    try:
                        self.socket.close()
                    except:
                        pass
                    self.socket = None

            # 创建新连接
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(10)
            print(f"正在连接到 {self.BEMFA_CONFIG['host']}:{self.BEMFA_CONFIG['port']}...")
            self.socket.connect((self.BEMFA_CONFIG["host"], self.BEMFA_CONFIG["port"]))
            
            # 发送认证信息（私钥）
            auth_msg = f"{self.BEMFA_CONFIG['uid']}\n"
            print(f"发送认证信息: {auth_msg.strip()}")
            self.socket.sendall(auth_msg.encode())
            self.connected = True
            print("巴法云TCP连接成功")
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            self.connected = False
            self.socket = None
            return False

    def subscribe(self):
        """订阅主题（巴法云TCP协议格式）"""
        if not self.connected or not self.socket:
            print("未连接，无法订阅")
            return False
        try:
            with self.connection_lock:
                # 订阅命令格式：cmd=1&topic=xxx&uid=xxx&type=3\n
                sub_cmd = (
                    f"cmd=1&topic={parse.quote(self.topic)}&"
                    f"uid={parse.quote(self.BEMFA_CONFIG['uid'])}&"
                    f"type={self.BEMFA_CONFIG['type']}\n"
                )
                print(f"发送订阅请求: {sub_cmd.strip()}")
                self.socket.sendall(sub_cmd.encode())
                print(f"已订阅主题: {self.topic}")
                return True
        except Exception as e:
            print(f"订阅失败: {e}")
            self.connected = False
            return False

    def start_heartbeat(self):
        """启动心跳线程（维持连接）"""
        def heartbeat_worker():
            while not self.stop_event.is_set() and self.connected:
                try:
                    with self.connection_lock:
                        if not self.socket:
                            print("心跳检测：socket已关闭")
                            self.connected = False
                            break
                            
                    # 心跳命令格式：cmd=0&uid=xxx&type=3\n
                    heartbeat_msg = (
                        f"cmd=0&uid={parse.quote(self.BEMFA_CONFIG['uid'])}&"
                        f"type={self.BEMFA_CONFIG['type']}\n"
                    )
                    self.socket.sendall(heartbeat_msg.encode())
                    time.sleep(self.BEMFA_CONFIG["heartbeat_interval"])
                except Exception as e:
                    print(f"心跳失败: {e}")
                    self.connected = False
                    break

        self.heartbeat_thread = threading.Thread(target=heartbeat_worker)
        self.heartbeat_thread.daemon = True
        self.heartbeat_thread.start()

    def start_receive(self):
        """启动接收线程（处理位置反馈）"""
        def receive_worker():
            buffer = ""
            while not self.stop_event.is_set() and self.connected:
                try:
                    with self.connection_lock:
                        if not self.socket:
                            print("接收线程：socket已关闭")
                            self.connected = False
                            break
                        
                        self.socket.settimeout(5)  # 设置较短的超时时间
                    data = self.socket.recv(1024).decode()
                        
                    if not data:
                        print("接收到空数据，连接可能已断开")
                        self.connected = False
                        break
                        
                    buffer += data
                    # 按换行符分割完整消息
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        self._process_data(line.strip())
                except socket.timeout:
                    # 超时不是错误，继续尝试
                    continue
                except Exception as e:
                    print(f"接收数据错误: {e}")
                    self.connected = False
                    break

        self.receive_thread = threading.Thread(target=receive_worker)
        self.receive_thread.daemon = True
        self.receive_thread.start()

    def _process_data(self, data):
        """解析巴法云返回的数据（提取位置信息）"""
        if not data:
            return
        try:
            print(f"收到原始数据: {data}")
            # 解析参数（格式：cmd=2&topic=xxx&msg=xxx&from=xxx）
            params = {}
            for part in data.split("&"):
                if "=" in part:
                    key, value = part.split("=", 1)
                    params[key] = parse.unquote(value)

            # 处理消息推送（cmd=2）
            if params.get("cmd") == "2":
                msg = params.get("msg", "")
                print(f"收到消息: {msg}")
                
                # 检查是否是控制AI发送的导航命令（主题为text3）
                if params.get("topic") == self.topic:
                    # 将导航命令传递给回调函数
                    if self.callback and msg.startswith("AREA_NAVIGATE:"):
                        self.callback({"command": msg})
                        return
                
                # 检查是否为UWB原始数据格式 [UWB-数据] 基站1:0x00000177,距离,角度,网格X,网格Y | 基站2:0x00000178,距离,角度,网格X,网格Y
                if "[UWB-数据]" in msg:
                    try:
                        # 提取基站数据部分
                        data_part = msg.split("[UWB-数据]")[1].strip()
                        
                        # 分割两个基站的数据
                        anchor_parts = data_part.split("|")
                        if len(anchor_parts) >= 2:
                            # 解析基站1数据
                            anchor1_info = anchor_parts[0].strip()
                            anchor1_match = anchor1_info.split("基站1:")[1].split(",")
                            if len(anchor1_match) >= 5:
                                anchor1_id = int(anchor1_match[0], 16) if "0x" in anchor1_match[0] else int(anchor1_match[0])
                                anchor1_distance = int(anchor1_match[1])
                                anchor1_azimuth = int(anchor1_match[2])
                                anchor1_grid_x = int(anchor1_match[3])
                                anchor1_grid_y = int(anchor1_match[4])
                                
                                anchor1_data = {
                                    "id": anchor1_id,
                                    "distance": anchor1_distance,
                                    "azimuth": anchor1_azimuth,
                                    "elevation": 0,  # 仰角数据可能不存在
                                    "grid_x": anchor1_grid_x,
                                    "grid_y": anchor1_grid_y
                                }
                                print(f"解析基站1数据: ID=0x{anchor1_id:X}, 距离={anchor1_distance}cm, 角度={anchor1_azimuth}°, 网格:({anchor1_grid_x},{anchor1_grid_y})")
                            
                            # 解析基站2数据
                            anchor2_info = anchor_parts[1].strip()
                            anchor2_match = anchor2_info.split("基站2:")[1].split(",")
                            if len(anchor2_match) >= 5:
                                anchor2_id = int(anchor2_match[0], 16) if "0x" in anchor2_match[0] else int(anchor2_match[0])
                                anchor2_distance = int(anchor2_match[1])
                                anchor2_azimuth = int(anchor2_match[2])
                                anchor2_grid_x = int(anchor2_match[3])
                                anchor2_grid_y = int(anchor2_match[4])
                                
                                anchor2_data = {
                                    "id": anchor2_id,
                                    "distance": anchor2_distance,
                                    "azimuth": anchor2_azimuth,
                                    "elevation": 0,  # 仰角数据可能不存在
                                    "grid_x": anchor2_grid_x,
                                    "grid_y": anchor2_grid_y
                                }
                                print(f"解析基站2数据: ID=0x{anchor2_id:X}, 距离={anchor2_distance}cm, 角度={anchor2_azimuth}°, 网格:({anchor2_grid_x},{anchor2_grid_y})")
                            
                            # 使用两个基站的网格坐标平均值作为小车位置
                            x = (int(anchor1_grid_x) + int(anchor2_grid_x)) // 2
                            y = (int(anchor1_grid_y) + int(anchor2_grid_y)) // 2
                            
                            # 使用当前朝向（如果有的话）
                            direction = 0
                            if self.current_position and len(self.current_position) >= 3:
                                direction = self.current_position[2]
                            
                            self.previous_position = self.current_position
                            self.current_position = (x, y, direction)
                            
                            # 组合数据并回调
                            self.waiting_for_position = False
                            if self.callback:
                                combined_data = {
                                    "position": self.current_position,
                                    "anchor1": anchor1_data,
                                    "anchor2": anchor2_data
                                }
                                self.callback(combined_data)  # 传递完整的双基站数据
                    except Exception as e:
                        print(f"UWB原始数据解析错误: {e}")
                        
                # 检查是否为标准位置信息（格式：pos:x,y,dir）
                elif msg.lower().startswith("pos:"):
                    try:
                        pos_str = msg[4:]  # 去掉"pos:"前缀
                        pos_parts = pos_str.split(",")
                        if len(pos_parts) == 3:
                            x = int(pos_parts[0])
                            y = int(pos_parts[1])
                            direction = int(pos_parts[2])
                            self.previous_position = self.current_position
                            self.current_position = (x, y, direction)
                            print(f"收到位置: ({x}, {y}, {direction})")
                            self.waiting_for_position = False
                            if self.callback:
                                self.callback(self.current_position)  # 触发回调更新UI
                    except ValueError as e:
                        print(f"位置数据格式错误: {pos_str}, 错误: {e}")
                
                # 检查是否为双基站位置信息（格式：pos2:x,y,dir|id1,dist1,angle1,elev1|id2,dist2,angle2,elev2）
                elif msg.lower().startswith("pos2:"):
                    try:
                        print(f"收到双基站位置数据: {msg}")
                        pos_str = msg[5:]  # 去掉"pos2:"前缀
                        parts = pos_str.split("|")
                        if len(parts) >= 3:
                            # 第一部分：小车位置
                            pos_parts = parts[0].split(",")
                            if len(pos_parts) >= 3:
                                x = int(pos_parts[0])
                                y = int(pos_parts[1])
                                direction = int(pos_parts[2])
                                self.previous_position = self.current_position
                                self.current_position = (x, y, direction)
                                print(f"收到双基站位置: ({x}, {y}, {direction})")
                            else:
                                print(f"位置部分格式错误: {parts[0]}, 应为x,y,dir")
                                return
                            
                            # 第二部分：基站1数据
                            anchor1_data = {}
                            anchor1_parts = parts[1].split(",")
                            if len(anchor1_parts) >= 4:
                                try:
                                    anchor1_data = {
                                        "id": int(anchor1_parts[0], 16) if anchor1_parts[0].startswith("0x") else int(anchor1_parts[0]),
                                        "distance": int(anchor1_parts[1]),
                                        "azimuth": int(anchor1_parts[2]),
                                        "elevation": int(anchor1_parts[3]) if len(anchor1_parts) > 3 else 0
                                    }
                                    print(f"基站1数据: ID=0x{anchor1_data['id']:X}, 距离={anchor1_data['distance']}cm, 角度={anchor1_data['azimuth']}°")
                                except ValueError as e:
                                    print(f"基站1数据解析错误: {parts[1]}, 错误: {e}")
                            else:
                                print(f"基站1数据格式错误: {parts[1]}, 应为id,distance,azimuth,elevation")
                            
                            # 第三部分：基站2数据
                            anchor2_data = {}
                            anchor2_parts = parts[2].split(",")
                            if len(anchor2_parts) >= 4:
                                try:
                                    anchor2_data = {
                                        "id": int(anchor2_parts[0], 16) if anchor2_parts[0].startswith("0x") else int(anchor2_parts[0]),
                                        "distance": int(anchor2_parts[1]),
                                        "azimuth": int(anchor2_parts[2]),
                                        "elevation": int(anchor2_parts[3]) if len(anchor2_parts) > 3 else 0
                                    }
                                    print(f"基站2数据: ID=0x{anchor2_data['id']:X}, 距离={anchor2_data['distance']}cm, 角度={anchor2_data['azimuth']}°")
                                except ValueError as e:
                                    print(f"基站2数据解析错误: {parts[2]}, 错误: {e}")
                            else:
                                print(f"基站2数据格式错误: {parts[2]}, 应为id,distance,azimuth,elevation")
                            
                            # 组合数据并回调
                            self.waiting_for_position = False
                            if self.callback:
                                combined_data = {
                                    "position": self.current_position,
                                    "anchor1": anchor1_data,
                                    "anchor2": anchor2_data
                                }
                                self.callback(combined_data)  # 传递完整的双基站数据
                    except Exception as e:
                        print(f"双基站位置数据解析错误: {msg}, 错误: {e}")
                        import traceback
                        traceback.print_exc()  # 打印详细的错误堆栈
        except Exception as e:
            print(f"数据解析错误: {e}")
            import traceback
            traceback.print_exc()  # 打印详细的错误堆栈

    def request_position(self):
        """请求小车当前位置（发送位置请求指令）"""
        if not self.connected:
            print("未连接，发送失败")
            return False
            
        if not self.socket:
            print("Socket未初始化，尝试重新连接...")
            if not self.connect():
                print("重连失败，无法发送位置请求")
                return False
            
        try:
            with self.connection_lock:
                if not self.socket:
                    print("Socket已关闭，无法发送位置请求")
                    return False
                    
                # 位置请求指令 - 使用单片机期望的格式
                msg_content = "position()"  # 单片机代码中期望的格式
                # 确保使用text3主题发送位置请求
                topic_to_use = "text3"
                pub_cmd = (
                    f"cmd=2&topic={topic_to_use}&"
                    f"msg={msg_content}&"
                    f"uid={self.BEMFA_CONFIG['uid']}&"
                    f"from={self.BEMFA_CONFIG['uid']}\n"
                )
                print(f"发送位置请求: {pub_cmd.strip()}")
                self.socket.sendall(pub_cmd.encode())
                self.waiting_for_position = True
                print(f"已发送位置请求: {msg_content}")
                return True
        except Exception as e:
            print(f"位置请求失败: {e}")
            self.connected = False
            return False

    def reset_direction(self):
        """重置小车方向（发送初始化指令）"""
        if not self.connected:
            print("未连接，发送失败")
            return False
            
        if not self.socket:
            print("Socket未初始化，尝试重新连接...")
            if not self.connect():
                print("重连失败，无法发送指令")
                return False
            
        try:
            with self.connection_lock:
                if not self.socket:
                    print("Socket已关闭，无法发送指令")
                    return False
                    
                # 初始化方向指令 - 直接使用明文
                msg_content = "init()"
                # 确保使用text3主题发送指令
                topic_to_use = "text3"
                pub_cmd = (
                    f"cmd=2&topic={topic_to_use}&"
                    f"msg={msg_content}&"
                    f"uid={self.BEMFA_CONFIG['uid']}&"
                    f"from={self.BEMFA_CONFIG['uid']}\n"
                )
                print(f"发送初始化方向指令: {pub_cmd.strip()}")
                self.socket.sendall(pub_cmd.encode())
                print("已发送初始化方向指令")
            return True
        except Exception as e:
            print(f"指令发送失败: {e}")
            self.connected = False
            return False
            
    def request_status(self):
        """请求小车状态"""
        if not self.connected:
            print("未连接，发送失败")
            return False
            
        if not self.socket:
            print("Socket未初始化，尝试重新连接...")
            if not self.connect():
                print("重连失败，无法发送指令")
                return False
            
        try:
            with self.connection_lock:
                if not self.socket:
                    print("Socket已关闭，无法发送指令")
                    return False
                    
                # 状态请求指令 - 直接使用明文
                msg_content = "status"
                # 确保使用text3主题发送指令
                topic_to_use = "text3"
                pub_cmd = (
                    f"cmd=2&topic={topic_to_use}&"
                    f"msg={msg_content}&"
                    f"uid={self.BEMFA_CONFIG['uid']}&"
                    f"from={self.BEMFA_CONFIG['uid']}\n"
                )
                print(f"发送状态请求: {pub_cmd.strip()}")
                self.socket.sendall(pub_cmd.encode())
                print("已发送状态请求")
                return True
        except Exception as e:
            print(f"指令发送失败: {e}")
            self.connected = False
            return False

    def send_command(self, command):
        """发送控制指令（与小车代码对应）"""
        if not self.connected:
            print("未连接，发送失败")
            return False
            
        if not self.socket:
            print("Socket未初始化，尝试重新连接...")
            if not self.connect():
                print("重连失败，无法发送指令")
                return False
            
        # 小车支持的指令格式：
        # left|move:3 - 先左转，然后前进3步
        # right|move:2 - 先右转，然后前进2步
        # back|move:1 - 先掉头，然后前进1步
        # move:4 - 直接前进4步（无前置指令）
        # position() - 查询位置
        # init() - 重置方向
        # status - 查询状态
        try:
            with self.connection_lock:
                if not self.socket:
                    print("Socket已关闭，无法发送控制指令")
                    return False
                
                # 特殊指令处理
                if command == "position()":
                    return self.request_position()
                elif command == "init()":
                    return self.reset_direction()
                elif command == "status":
                    return self.request_status()
                
                # 处理标准控制指令
                formatted_command = ""
                
                # 检查是否是组合指令（前置指令|控制指令）
                if "|" in command:
                    parts = command.split("|")
                    if len(parts) == 2:
                        pre_cmd = parts[0].strip().lower()
                        move_cmd = parts[1].strip().lower()
                        
                        # 验证前置指令
                        if pre_cmd in ["left", "right", "back"]:
                            # 验证移动指令
                            if move_cmd.startswith("move:"):
                                try:
                                    steps = int(move_cmd.split(":")[1])
                                    if steps > 0:
                                        formatted_command = f"{pre_cmd}|move:{steps}"
                                    else:
                                        # 步数无效，使用默认值
                                        formatted_command = f"{pre_cmd}|move:1"
                                except:
                                    # 解析错误，使用默认值
                                    formatted_command = f"{pre_cmd}|move:1"
                            else:
                                # 如果没有有效的移动指令，添加默认移动
                                formatted_command = f"{pre_cmd}|move:1"
                        else:
                            # 无效的前置指令，使用默认移动
                            formatted_command = "move:1"
                else:
                    # 单一指令处理
                    cmd = command.strip().lower()
                    
                    # 处理移动指令
                    if cmd.startswith("move:"):
                        try:
                            steps = int(cmd.split(":")[1])
                            if steps > 0:
                                formatted_command = f"move:{steps}"
                            else:
                                formatted_command = "move:1"  # 默认前进一步
                        except:
                            formatted_command = "move:1"  # 默认前进一步
                    # 处理方向指令 - 不允许单独发送，必须添加移动
                    elif cmd in ["left", "right", "back"]:
                        formatted_command = f"{cmd}|move:1"  # 添加默认移动
                    else:
                        formatted_command = "move:1"  # 默认前进一步
                
                # 确保使用text3主题发送指令
                topic_to_use = "text3"
                # 直接使用明文，不使用parse.quote编码
                pub_cmd = (
                    f"cmd=2&topic={topic_to_use}&"
                    f"msg={formatted_command}&"
                    f"uid={self.BEMFA_CONFIG['uid']}&"
                    f"from={self.BEMFA_CONFIG['uid']}\n"
                )
                print(f"发送控制指令: {pub_cmd.strip()}")
                self.socket.sendall(pub_cmd.encode())
                print(f"发送指令: {formatted_command}")
                return True
        except Exception as e:
            print(f"指令发送失败: {e}")
            self.connected = False
            return False

    def start(self):
        """启动客户端（连接+订阅+线程）"""
        if self.connect() and self.subscribe():
            self.stop_event.clear()
            self.start_heartbeat()
            self.start_receive()
            return True
        return False

    def stop(self):
        """停止连接"""
        self.stop_event.set()  # 设置停止标志
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
        print("巴法云连接已关闭")

    def reconnect(self):
        """重新连接"""
        print("尝试重新连接...")
        self.stop()
        time.sleep(1)  # 等待1秒
        return self.start()

    # 添加callback属性的setter方法，使其支持回调函数
    def set_callback(self, callback_func):
        """设置位置更新回调函数"""
        self.callback = callback_func

    def send_tts_message(self, message):
        """发送TTS消息到小车（使用text3主题）"""
        if not self.connected:
            print("未连接，发送失败")
            return False
            
        if not self.socket:
            print("Socket未初始化，尝试重新连接...")
            if not self.connect():
                print("重连失败，无法发送TTS消息")
                return False
            
        try:
            with self.connection_lock:
                if not self.socket:
                    print("Socket已关闭，无法发送TTS消息")
                    return False
                    
                # 添加TTS前缀
                tts_message = f"tts:{message}"
                # 确保使用text3主题发送TTS消息
                topic_to_use = "text3"
                pub_cmd = (
                    f"cmd=2&topic={topic_to_use}&"
                    f"msg={tts_message}&"
                    f"uid={self.BEMFA_CONFIG['uid']}&"
                    f"from={self.BEMFA_CONFIG['uid']}\n"
                )
                print(f"发送TTS消息: {tts_message}")
                self.socket.sendall(pub_cmd.encode())
                return True
        except Exception as e:
            print(f"TTS消息发送失败: {e}")
            self.connected = False
            return False


# 基站朝向枚举，8个方向
class AnchorDirection:
    NORTH = 0          # 北
    NORTHEAST = 1      # 东北
    EAST = 2           # 东
    SOUTHEAST = 3      # 东南
    SOUTH = 4          # 南
    SOUTHWEST = 5      # 西南
    WEST = 6           # 西
    NORTHWEST = 7      # 西北
    
    # 方向对应的角度(以y轴北方向为0度，顺时针旋转)
    ANGLES = [0, 45, 90, 135, 180, 225, 270, 315]
    
    # 方向名称
    NAMES = ["北", "东北", "东", "东南", "南", "西南", "西", "西北"]

# 标记区域类，用于保存特殊区域的信息
class MarkedArea:
    def __init__(self, name, color="lightblue"):
        self.name = name            # 区域名称
        self.color = color          # 区域颜色
        self.cells = []             # 区域包含的单元格列表 [(x1,y1), (x2,y2), ...]
        
    def add_cell(self, x, y):
        """添加一个单元格到区域"""
        if (x, y) not in self.cells:
            self.cells.append((x, y))
            return True
        return False
        
    def remove_cell(self, x, y):
        """从区域移除一个单元格"""
        if (x, y) in self.cells:
            self.cells.remove((x, y))
            return True
        return False
        
    def contains(self, x, y):
        """检查点(x,y)是否在区域内"""
        return (x, y) in self.cells
        
    def get_center(self):
        """获取区域中心点"""
        if not self.cells:
            return (0, 0)
            
        # 计算所有单元格的平均位置
        sum_x = sum(cell[0] for cell in self.cells)
        sum_y = sum(cell[1] for cell in self.cells)
        return (int(sum_x / len(self.cells)), int(sum_y / len(self.cells)))
        
    def get_bounds(self):
        """获取区域的边界 (min_x, min_y, max_x, max_y)"""
        if not self.cells:
            return (0, 0, 0, 0)
            
        min_x = min(cell[0] for cell in self.cells)
        min_y = min(cell[1] for cell in self.cells)
        max_x = max(cell[0] for cell in self.cells)
        max_y = max(cell[1] for cell in self.cells)
        return (min_x, min_y, max_x, max_y)
        
    def to_dict(self):
        """转换为字典，用于保存"""
        return {
            "name": self.name,
            "color": self.color,
            "cells": self.cells,
            "center": self.get_center(),
            "bounds": self.get_bounds()
        }
        
    @classmethod
    def from_dict(cls, data):
        """从字典创建对象，用于加载"""
        area = cls(data["name"], data.get("color", "lightblue"))
        
        # 确保cells是列表形式的坐标点
        if "cells" in data and isinstance(data["cells"], list):
            # 确保每个cell是(x,y)格式的元组
            for cell in data["cells"]:
                if isinstance(cell, list) and len(cell) == 2:
                    # 如果是列表格式[x,y]，转换为元组(x,y)
                    area.add_cell(cell[0], cell[1])
                elif isinstance(cell, tuple) and len(cell) == 2:
                    # 如果已经是元组格式(x,y)
                    area.add_cell(cell[0], cell[1])
                    
        print(f"从配置加载区域: {area.name}, 颜色: {area.color}, 单元格数: {len(area.cells)}")
        return area

# 基站配置类
class AnchorConfig:
    def __init__(self, anchor_id, x=0, y=0, direction=AnchorDirection.NORTH):
        self.anchor_id = anchor_id      # 基站ID
        self.x = x                      # 基站在画布上的X坐标
        self.y = y                      # 基站在画布上的Y坐标
        self.direction = direction      # 基站朝向(0-7)
        self.distance = 0               # 最近一次测量的距离
        self.azimuth = 0                # 最近一次测量的方位角
        self.elevation = 0              # 仰角
        self.grid_x = 0                 # 相对基站的网格X
        self.grid_y = 0                 # 相对基站的网格Y
        self.last_update_time = 0.0     # 最后更新时间(浮点型)
        self.weight = 0.0               # 权重(0.0-1.0)
        self.data_ready = False         # 数据是否就绪
        
    # 将基站测量的相对坐标转换为画布上的绝对坐标
    def convert_to_absolute(self, grid_size=15):
        """
        将基站测量的相对坐标转换为画布上的绝对坐标
        grid_size: 网格大小(厘米)
        返回: (绝对x坐标, 绝对y坐标)
        """
        if not self.data_ready:
            print(f"基站ID:0x{self.anchor_id:X}数据未就绪，无法转换坐标")
            return (0, 0)
        
        # 简化坐标转换逻辑
        # 1. 如果基站已经提供了相对网格坐标(grid_x, grid_y)，直接使用
        # 2. 根据基站位置和朝向进行转换
        
        # 基站朝向的角度
        base_angle = AnchorDirection.ANGLES[self.direction]
        print(f"基站ID:0x{self.anchor_id:X} 朝向: {AnchorDirection.NAMES[self.direction]}({base_angle}°)")
        
        # 如果有网格坐标，直接使用
        if hasattr(self, 'grid_x') and hasattr(self, 'grid_y') and self.grid_x != 0 and self.grid_y != 0:
            # 根据基站朝向调整坐标
            # 基站朝向为NORTH(0°)时，基站坐标系y轴正方向与画布y轴正方向一致
            # 基站朝向为EAST(90°)时，基站坐标系y轴正方向与画布x轴正方向一致
            # 基站朝向为SOUTH(180°)时，基站坐标系y轴正方向与画布y轴负方向一致
            # 基站朝向为WEST(270°)时，基站坐标系y轴正方向与画布x轴负方向一致
            
            # 根据基站朝向旋转网格坐标
            rotated_x = 0
            rotated_y = 0
            
            if base_angle == 0:  # NORTH
                rotated_x = self.grid_x
                rotated_y = self.grid_y
            elif base_angle == 90:  # EAST
                rotated_x = self.grid_y
                rotated_y = -self.grid_x
            elif base_angle == 180:  # SOUTH
                rotated_x = -self.grid_x
                rotated_y = -self.grid_y
            elif base_angle == 270:  # WEST
                rotated_x = -self.grid_y
                rotated_y = self.grid_x
            else:  # 对角线方向需要使用三角函数计算
                angle_rad = math.radians(base_angle)
                rotated_x = self.grid_x * math.cos(angle_rad) - self.grid_y * math.sin(angle_rad)
                rotated_y = self.grid_x * math.sin(angle_rad) + self.grid_y * math.cos(angle_rad)
            
            # 计算绝对坐标 = 基站位置 + 旋转后的相对坐标
            abs_x = self.x + rotated_x
            abs_y = self.y + rotated_y
            
            print(f"基站ID:0x{self.anchor_id:X} 坐标转换: 基站位置({self.x},{self.y}), 网格坐标:({self.grid_x},{self.grid_y})")
            print(f"旋转后坐标: ({rotated_x:.2f},{rotated_y:.2f}), 绝对坐标: ({abs_x:.2f},{abs_y:.2f})")
            
            return (abs_x, abs_y)
        
        # 如果没有网格坐标，使用距离和角度计算
        # 将方位角转换为弧度，方位角是相对于基站朝向的y轴
        azimuth_rad = math.radians(self.azimuth)
        
        # 计算全局角度 = 基站朝向角度 + 测量方位角
        global_angle_rad = math.radians(base_angle) + azimuth_rad
        
        # 根据距离和全局角度计算偏移量(厘米)
        x_offset = self.distance * math.sin(global_angle_rad)
        y_offset = self.distance * math.cos(global_angle_rad)
        
        # 转换到网格坐标
        grid_x_offset = x_offset / grid_size
        grid_y_offset = y_offset / grid_size
        
        # 计算绝对网格坐标(以基站位置为起点)
        abs_grid_x = self.x + grid_x_offset
        abs_grid_y = self.y + grid_y_offset
        
        print(f"基站ID:0x{self.anchor_id:X} 坐标转换: 基站位置({self.x},{self.y}), 距离={self.distance}cm, 角度={self.azimuth}°")
        print(f"偏移量: ({grid_x_offset:.2f},{grid_y_offset:.2f}), 绝对坐标: ({abs_grid_x:.2f},{abs_grid_y:.2f})")
        
        return (abs_grid_x, abs_grid_y)
        
    def to_dict(self):
        """转换为字典，用于保存"""
        return {
            "anchor_id": self.anchor_id,
            "x": self.x,
            "y": self.y,
            "direction": self.direction
        }
        
    @classmethod
    def from_dict(cls, data):
        """从字典创建对象，用于加载"""
        return cls(
            data["anchor_id"],
            data["x"],
            data["y"],
            data["direction"]
        )


class RobotNavigationApp:
    def __init__(self, root, grid_size=20):
        self.root = root
        self.root.title("机器人导航系统（巴法云TCP版）- 双基站版")
        self.grid_size = grid_size
        self.rows = 20
        self.cols = 20
        self.grid = [[0 for _ in range(self.cols)] for _ in range(self.rows)]  # 0:空白,1:障碍物
        self.start_pos = None  # 起点由小车当前位置决定
        self.goal_pos = (self.rows-1, self.cols-1)  # 默认终点
        self.current_pos = None
        self.previous_pos = None  # 用于推断朝向
        self.path = []
        self.direction = 0  # 0:北,1:东,2:南,3:西
        self.navigation_active = False
        self.waiting_for_response = False
        self.last_command_time = 0  # 记录最后一次发送指令的时间
        self.calculated_direction = 0  # 根据连续位置计算的朝向
        
        # 基站配置
        self.anchor1 = AnchorConfig(0x177, x=3, y=3, direction=AnchorDirection.EAST)  # 默认基站1放在(3,3)位置
        self.anchor2 = AnchorConfig(0x178, x=15, y=3, direction=AnchorDirection.WEST)  # 默认基站2放在(15,3)位置
        self.anchors = [self.anchor1, self.anchor2]  # 基站列表
        self.anchor_config_mode = False  # 基站配置模式标志
        self.selected_anchor = None      # 当前选中的基站
        
        # 权重配置
        self.weight_mode = "dynamic"     # 权重模式: "fixed"(固定) 或 "dynamic"(动态)
        self.fixed_weight = 1.0          # 固定权重比例(基站1的权重，基站2权重为1-此值)
        self.active_anchor = self.anchor1  # 当前主导基站(权重为1的基站)
        
        # 标记区域相关
        self.marked_areas = []           # 标记区域列表
        self.marking_mode = False        # 标记模式标志
        self.marking_start = None        # 标记起点
        self.marking_current = None      # 当前标记点
        self.hover_info = None           # 鼠标悬停信息
        self.highlighted_area = None     # 当前高亮显示的区域
        self.show_all_areas = True       # 是否显示所有区域
        
        # 保存/加载配置文件路径
        self.config_file = "navigation_config.json"
        
        # 巴法云客户端（使用text3主题与小车对应）
        self.bf_client = BFYunClient(topic="text3")
        self.bf_client.set_callback(self.update_position)
        
        # 创建UI
        self.create_widgets()
        
        # 更新区域列表显示
        self.update_area_listbox()
        
        # 尝试加载配置
        config_loaded = self.load_config()
        
        # 如果配置加载成功并且有标记区域，确保区域可见
        if config_loaded and self.marked_areas:
            # 确保区域可见
            self.show_all_areas = True
            if hasattr(self, 'show_all_areas_var'):
                self.show_all_areas_var.set(True)
                
            # 确保有选中的区域
            if not self.highlighted_area and self.marked_areas:
                self.highlighted_area = self.marked_areas[0]
                self.current_area = self.marked_areas[0]
                
            # 更新UI
            if hasattr(self, 'area_listbox') and self.highlighted_area:
                # 找到高亮区域的索引
                try:
                    index = self.marked_areas.index(self.highlighted_area)
                    self.area_listbox.selection_clear(0, tk.END)
                    self.area_listbox.selection_set(index)
                except ValueError:
                    # 如果找不到索引，选择第一个
                    if self.marked_areas:
                        self.area_listbox.selection_clear(0, tk.END)
                        self.area_listbox.selection_set(0)
                        self.highlighted_area = self.marked_areas[0]
                        self.current_area = self.marked_areas[0]
                
            if hasattr(self, 'current_area_var') and self.current_area:
                self.current_area_var.set(f"当前区域: {self.current_area.name}")
                
            if hasattr(self, 'area_info_var') and self.current_area:
                center_x, center_y = self.current_area.get_center()
                self.area_info_var.set(f"区域信息: {self.current_area.name} 中心点:({center_x},{center_y}) 单元格数:{len(self.current_area.cells)}")
                
            # 切换到标记区域标签页
            if hasattr(self, 'notebook'):
                try:
                    self.notebook.select(2)  # 索引2是标记区域标签页
                except Exception as e:
                    print(f"自动切换标签页失败: {e}")
        
        # 绘制网格
        self.draw_grid()

    def create_widgets(self):
        # 画布
        self.canvas = tk.Canvas(
            self.root, 
            width=self.cols*self.grid_size, 
            height=self.rows*self.grid_size, 
            bg="white"
        )
        self.canvas.pack(side=tk.LEFT, padx=10, pady=10)
        self.canvas.bind("<Button-1>", self.on_canvas_click)  # 点击设置终点或基站
        self.canvas.bind("<B1-Motion>", self.on_canvas_drag)  # 拖动绘制标记区域
        self.canvas.bind("<ButtonRelease-1>", self.on_canvas_release)  # 释放鼠标完成标记
        self.canvas.bind("<Motion>", self.on_canvas_motion)  # 鼠标移动显示区域信息
        
        # 控制面板（使用Notebook选项卡）
        self.control_frame = tk.Frame(self.root)
        self.control_frame.pack(side=tk.RIGHT, padx=10, pady=10, fill=tk.BOTH, expand=True)
        
        # 创建选项卡
        self.notebook = ttk.Notebook(self.control_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)
        
        # 导航选项卡
        self.nav_tab = tk.Frame(self.notebook)
        self.notebook.add(self.nav_tab, text="导航控制")
        
        # 基站配置选项卡
        self.anchor_tab = tk.Frame(self.notebook)
        self.notebook.add(self.anchor_tab, text="基站配置")
        
        # 标记区域选项卡
        self.mark_tab = tk.Frame(self.notebook)
        self.notebook.add(self.mark_tab, text="标记区域")
        
        # 添加标签页切换事件
        self.notebook.bind("<<NotebookTabChanged>>", self.on_tab_changed)
        
        # === 导航选项卡内容 ===
        # 巴法云配置（预设私钥，直接使用）
        tk.Label(self.nav_tab, text="巴法云私钥（已预设）:").pack(anchor=tk.W)
        self.uid_entry = tk.Entry(self.nav_tab, width=30)
        self.uid_entry.insert(0, "11ca31c562d14ef786a0238a03a3be16")  # 控制台私钥
        self.uid_entry.pack(anchor=tk.W)
        self.connect_btn = tk.Button(self.nav_tab, text="连接巴法云", command=self.connect_bfyun)
        self.connect_btn.pack(anchor=tk.W, pady=5)
        
        # 重连按钮
        self.reconnect_btn = tk.Button(self.nav_tab, text="重新连接", command=self.reconnect_bfyun, state=tk.DISABLED)
        self.reconnect_btn.pack(anchor=tk.W, pady=5)
        
        # 状态显示
        self.status_var = tk.StringVar(value="未连接")
        tk.Label(self.nav_tab, text="状态:").pack(anchor=tk.W)
        tk.Label(self.nav_tab, textvariable=self.status_var).pack(anchor=tk.W)
        
        # 当前位置坐标显示
        self.position_var = tk.StringVar(value="当前位置：未知")
        tk.Label(self.nav_tab, textvariable=self.position_var).pack(anchor=tk.W, pady=5)
        
        # 终点坐标显示
        self.goal_pos_var = tk.StringVar(value=f"终点位置：({self.goal_pos[1]}, {self.goal_pos[0]})")
        tk.Label(self.nav_tab, textvariable=self.goal_pos_var).pack(anchor=tk.W, pady=5)
        
        # 控制按钮
        self.get_pos_btn = tk.Button(self.nav_tab, text="获取当前位置", command=self.request_position, state=tk.DISABLED)
        self.get_pos_btn.pack(pady=5, fill=tk.X)
        
        self.plan_btn = tk.Button(self.nav_tab, text="规划路径", command=self.plan_path, state=tk.DISABLED)
        self.plan_btn.pack(pady=5, fill=tk.X)
        
        self.start_btn = tk.Button(self.nav_tab, text="开始导航", command=self.start_navigation, state=tk.DISABLED)
        self.start_btn.pack(pady=5, fill=tk.X)
        
        self.stop_btn = tk.Button(self.nav_tab, text="停止导航", command=self.stop_navigation, state=tk.DISABLED)
        self.stop_btn.pack(pady=5, fill=tk.X)
        
        # 设置障碍物按钮
        self.obstacle_mode_var = tk.BooleanVar(value=False)
        self.obstacle_btn = tk.Checkbutton(
            self.nav_tab, 
            text="障碍物设置模式", 
            variable=self.obstacle_mode_var
        )
        self.obstacle_btn.pack(pady=5, anchor=tk.W)
        
        # === 基站配置选项卡内容 ===
        # 基站模式切换
        self.anchor_mode_var = tk.BooleanVar(value=False)
        self.anchor_config_btn = tk.Checkbutton(
            self.anchor_tab,
            text="基站配置模式",
            variable=self.anchor_mode_var,
            command=self.toggle_anchor_mode
        )
        self.anchor_config_btn.pack(pady=5, anchor=tk.W)
        
        # 权重配置
        weight_frame = tk.Frame(self.anchor_tab)
        weight_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(weight_frame, text="权重模式:").pack(side=tk.LEFT)
        self.weight_mode_var = tk.StringVar(value=self.weight_mode)
        self.weight_mode_menu = tk.OptionMenu(weight_frame, self.weight_mode_var, 
                                            "fixed", "dynamic", 
                                            command=self.change_weight_mode)
        self.weight_mode_menu.pack(side=tk.LEFT, padx=5)
        
        tk.Label(weight_frame, text="固定权重比:").pack(side=tk.LEFT, padx=(10,0))
        self.weight_scale = tk.Scale(weight_frame, from_=0, to=100, 
                                   orient=tk.HORIZONTAL, length=150,
                                   command=self.update_weight_value)
        self.weight_scale.set(int(self.fixed_weight * 100))
        self.weight_scale.pack(side=tk.LEFT, padx=5)
        
        self.weight_label = tk.Label(weight_frame, text=f"{int(self.fixed_weight * 100)}:{int((1-self.fixed_weight) * 100)}")
        self.weight_label.pack(side=tk.LEFT)
        
        # 基站1配置
        tk.Label(self.anchor_tab, text="基站1 (ID:0x177)配置:").pack(anchor=tk.W, pady=(10,0))
        
        anchor1_frame = tk.Frame(self.anchor_tab)
        anchor1_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(anchor1_frame, text="位置:").pack(side=tk.LEFT)
        self.anchor1_pos_label = tk.Label(anchor1_frame, text=f"({self.anchor1.x},{self.anchor1.y})")
        self.anchor1_pos_label.pack(side=tk.LEFT, padx=5)
        
        tk.Label(anchor1_frame, text="朝向:").pack(side=tk.LEFT, padx=(10,0))
        self.anchor1_dir_var = tk.StringVar(value=AnchorDirection.NAMES[self.anchor1.direction])
        self.anchor1_dir_menu = tk.OptionMenu(anchor1_frame, self.anchor1_dir_var, 
                                           *AnchorDirection.NAMES, 
                                           command=lambda v: self.change_anchor_direction(self.anchor1, v))
        self.anchor1_dir_menu.pack(side=tk.LEFT)
        
        self.select_anchor1_btn = tk.Button(anchor1_frame, text="选择", command=lambda: self.select_anchor(self.anchor1))
        self.select_anchor1_btn.pack(side=tk.LEFT, padx=10)
        
        # 基站2配置
        tk.Label(self.anchor_tab, text="基站2 (ID:0x178)配置:").pack(anchor=tk.W, pady=(10,0))
        
        anchor2_frame = tk.Frame(self.anchor_tab)
        anchor2_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(anchor2_frame, text="位置:").pack(side=tk.LEFT)
        self.anchor2_pos_label = tk.Label(anchor2_frame, text=f"({self.anchor2.x},{self.anchor2.y})")
        self.anchor2_pos_label.pack(side=tk.LEFT, padx=5)
        
        tk.Label(anchor2_frame, text="朝向:").pack(side=tk.LEFT, padx=(10,0))
        self.anchor2_dir_var = tk.StringVar(value=AnchorDirection.NAMES[self.anchor2.direction])
        self.anchor2_dir_menu = tk.OptionMenu(anchor2_frame, self.anchor2_dir_var, 
                                           *AnchorDirection.NAMES, 
                                           command=lambda v: self.change_anchor_direction(self.anchor2, v))
        self.anchor2_dir_menu.pack(side=tk.LEFT)
        
        self.select_anchor2_btn = tk.Button(anchor2_frame, text="选择", command=lambda: self.select_anchor(self.anchor2))
        self.select_anchor2_btn.pack(side=tk.LEFT, padx=10)
        
        # 基站数据显示区域
        tk.Label(self.anchor_tab, text="基站数据:").pack(anchor=tk.W, pady=(10,0))
        self.anchor_data_text = tk.Text(self.anchor_tab, height=8, width=35)
        self.anchor_data_text.pack(fill=tk.BOTH, expand=True, pady=5)
        self.anchor_data_text.insert(tk.END, "等待基站数据...\n")
        self.anchor_data_text.config(state=tk.DISABLED)
        
        # === 标记区域选项卡内容 ===
        # 区域显示控制
        display_frame = tk.Frame(self.mark_tab)
        display_frame.pack(fill=tk.X, pady=5)
        
        self.show_all_areas_var = tk.BooleanVar(value=self.show_all_areas)
        self.show_all_areas_cb = tk.Checkbutton(
            display_frame, 
            text="显示所有区域", 
            variable=self.show_all_areas_var,
            command=self.toggle_area_display
        )
        self.show_all_areas_cb.pack(side=tk.LEFT)
        
        # 标记模式切换
        self.marking_mode_var = tk.BooleanVar(value=False)
        self.marking_btn = tk.Checkbutton(
            self.mark_tab,
            text="标记区域模式",
            variable=self.marking_mode_var,
            command=self.toggle_marking_mode
        )
        self.marking_btn.pack(pady=5, anchor=tk.W)
        
        # 标记区域颜色选择
        color_frame = tk.Frame(self.mark_tab)
        color_frame.pack(fill=tk.X, pady=5)
        
        tk.Label(color_frame, text="区域颜色:").pack(side=tk.LEFT)
        self.color_var = tk.StringVar(value="lightblue")
        self.color_menu = tk.OptionMenu(color_frame, self.color_var, 
                                      "lightblue", "lightgreen", "lightyellow", "lightpink", "lightcoral")
        self.color_menu.pack(side=tk.LEFT, padx=5)
        
        # 当前选中的区域
        self.current_area = None
        self.current_area_var = tk.StringVar(value="当前区域: 无")
        tk.Label(self.mark_tab, textvariable=self.current_area_var).pack(anchor=tk.W, pady=5)
        
        # 创建新区域按钮
        self.new_area_btn = tk.Button(self.mark_tab, text="创建新区域", command=self.create_new_area)
        self.new_area_btn.pack(pady=5, fill=tk.X)
        
        # 标记区域列表
        tk.Label(self.mark_tab, text="已标记区域:").pack(anchor=tk.W, pady=(10,0))
        
        # 创建一个带滚动条的列表框
        list_frame = tk.Frame(self.mark_tab)
        list_frame.pack(fill=tk.BOTH, expand=True, pady=5)
        
        self.area_listbox = tk.Listbox(list_frame, height=8)
        self.area_listbox.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        
        scrollbar = tk.Scrollbar(list_frame)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.area_listbox.config(yscrollcommand=scrollbar.set)
        scrollbar.config(command=self.area_listbox.yview)
        
        # 选择区域后显示详细信息
        self.area_listbox.bind("<<ListboxSelect>>", self.on_area_select)
        
        # 区域操作按钮
        btn_frame = tk.Frame(self.mark_tab)
        btn_frame.pack(fill=tk.X, pady=5)
        
        self.rename_btn = tk.Button(btn_frame, text="重命名", command=self.rename_area)
        self.rename_btn.pack(side=tk.LEFT, padx=5)
        
        self.delete_btn = tk.Button(btn_frame, text="删除", command=self.delete_area)
        self.delete_btn.pack(side=tk.LEFT, padx=5)
        
        self.goto_btn = tk.Button(btn_frame, text="导航至此", command=self.navigate_to_area)
        self.goto_btn.pack(side=tk.LEFT, padx=5)
        
        # 区域信息显示
        self.area_info_var = tk.StringVar(value="区域信息: 未选择")
        tk.Label(self.mark_tab, textvariable=self.area_info_var).pack(anchor=tk.W, pady=5)
        
        # 鼠标悬停信息显示
        self.hover_info_var = tk.StringVar(value="鼠标位置: -")
        tk.Label(self.mark_tab, textvariable=self.hover_info_var).pack(anchor=tk.W, pady=5)
        
        # 保存/加载按钮
        save_frame = tk.Frame(self.mark_tab)
        save_frame.pack(fill=tk.X, pady=10)
        
        self.save_btn = tk.Button(save_frame, text="保存配置", command=self.save_config)
        self.save_btn.pack(side=tk.LEFT, padx=5)
        
        self.load_btn = tk.Button(save_frame, text="加载配置", command=self.load_config)
        self.load_btn.pack(side=tk.LEFT, padx=5)

    def toggle_area_display(self):
        """切换区域显示模式"""
        self.show_all_areas = self.show_all_areas_var.get()
        self.draw_grid()
        
    def connect_bfyun(self):
        """连接巴法云"""
        uid = self.uid_entry.get().strip()
        if not uid:
            messagebox.showerror("错误", "私钥不能为空")
            return
        if self.bf_client.start():
            self.status_var.set("已连接到巴法云（主题：text1）")
            self.get_pos_btn.config(state=tk.NORMAL)
            self.reconnect_btn.config(state=tk.NORMAL)
        else:
            self.status_var.set("连接失败，请检查私钥")
            
    def reconnect_bfyun(self):
        """重新连接巴法云"""
        if self.bf_client.reconnect():
            self.status_var.set("已重新连接到巴法云")
            self.get_pos_btn.config(state=tk.NORMAL)
        else:
            self.status_var.set("重连失败，请检查网络")

    def on_canvas_click(self, event):
        """点击画布设置终点或障碍物或标记区域"""
        # 计算点击位置对应的网格坐标（原点在左下角）
        canvas_y = event.y
        canvas_x = event.x
        
        # 转换为网格坐标
        col = canvas_x // self.grid_size
        # 反转y轴，使原点在左下角
        row = self.rows - 1 - (canvas_y // self.grid_size)
        
        print(f"点击位置: 画布坐标({canvas_x}, {canvas_y}), 网格坐标({col}, {row})")
        
        if 0 <= row < self.rows and 0 <= col < self.cols:
            # 标记区域模式
            if self.marking_mode:
                # 确保有当前区域
                if self.current_area is None:
                    messagebox.showinfo("提示", "请先创建一个新区域")
                    self.create_new_area()
                    return
                    
                # 检查是否已在区域内
                if self.current_area.contains(col, row):
                    # 如果已在区域内，则移除
                    self.current_area.remove_cell(col, row)
                    self.status_var.set(f"从区域 {self.current_area.name} 移除单元格 ({col},{row})")
                else:
                    # 否则添加到区域
                    self.current_area.add_cell(col, row)
                    self.status_var.set(f"添加单元格 ({col},{row}) 到区域 {self.current_area.name}")
                
                # 重绘网格
                self.draw_grid()
                return
                
            # 基站配置模式
            if self.anchor_config_mode:
                # 只有在选中基站时才进行操作
                if self.selected_anchor:
                    # 更新基站位置
                    self.selected_anchor.x = col
                    self.selected_anchor.y = row
                    
                    # 更新UI
                    if self.selected_anchor == self.anchor1:
                        self.anchor1_pos_label.config(text=f"({col},{row})")
                    else:
                        self.anchor2_pos_label.config(text=f"({col},{row})")
                    
                    # 更新状态信息
                    self.status_var.set(f"基站ID:0x{self.selected_anchor.anchor_id:X}位置已设为({col},{row})")
                    print(f"基站ID:0x{self.selected_anchor.anchor_id:X}位置已设为({col},{row})，朝向:{AnchorDirection.NAMES[self.selected_anchor.direction]}")
                    print("提示：请确保设置正确的基站朝向，以便准确计算坐标")
                    
                    # 取消选中状态
                    self.selected_anchor = None
                    self.select_anchor1_btn.config(relief=tk.RAISED, bg="SystemButtonFace")
                    self.select_anchor2_btn.config(relief=tk.RAISED, bg="SystemButtonFace")
                else:
                    # 提示用户需要先选择基站
                    self.status_var.set("请先在右侧选择要配置的基站")
                
                self.draw_grid()
                self.update_anchor_data_display()
                return
            
            # 障碍物设置模式
            if self.obstacle_mode_var.get():
                # 跳过起点和终点
                if (row, col) == self.start_pos or (row, col) == self.goal_pos:
                    return
                # 切换障碍物状态
                self.grid[row][col] = 1 - self.grid[row][col]
            else:
                # 设置终点模式
                if self.start_pos and (row, col) == self.start_pos:
                    messagebox.showinfo("提示", "不能将终点设为起点位置")
                    return
                
                # 清除旧终点标记
                if self.goal_pos:
                    old_row, old_col = self.goal_pos
                    if 0 <= old_row < self.rows and 0 <= old_col < self.cols:
                        self.grid[old_row][old_col] = 0
                
                # 设置新终点
                self.goal_pos = (row, col)
                self.grid[row][col] = 3  # 终点标记
                
                # 更新右侧终点坐标显示
                self.goal_pos_var.set(f"终点位置：({col}, {row})")
                print(f"设置终点位置为: ({col}, {row})")
                
                # 如果已有起点，允许规划路径
                if self.start_pos:
                    self.plan_btn.config(state=tk.NORMAL)
            
            self.draw_grid()

    def draw_grid(self):
        """绘制网格和路径"""
        self.canvas.delete("all")
        
        # 定义坐标轴样式
        grid_color = "lightgray"  # 网格线颜色
        axis_color = "red"  # 主坐标轴颜色
        axis_width = 2  # 主坐标轴粗细
        label_font = ("Arial", 9, "bold")  # 坐标标签字体
        
        # 首先绘制网格
        for i in range(self.rows + 1):
            y = i * self.grid_size
            self.canvas.create_line(0, y, self.cols * self.grid_size, y, fill=grid_color, width=1)
            
        for j in range(self.cols + 1):
            x = j * self.grid_size
            self.canvas.create_line(x, 0, x, self.rows * self.grid_size, fill=grid_color, width=1)
            
        # 绘制主X轴和Y轴（粗线）
        # X轴 - 底部
        self.canvas.create_line(0, self.rows * self.grid_size, self.cols * self.grid_size, self.rows * self.grid_size, 
                               fill=axis_color, width=axis_width, arrow=tk.LAST)
        # Y轴 - 左侧                       
        self.canvas.create_line(0, self.rows * self.grid_size, 0, 0, 
                               fill=axis_color, width=axis_width, arrow=tk.LAST)
                               
        # 添加坐标标签
        for i in range(self.rows):
            # Y轴坐标标签 - 从下到上递增
            y_pos = self.rows * self.grid_size - i * self.grid_size - self.grid_size//2
            self.canvas.create_text(5, y_pos, 
                                  text=str(i), fill=axis_color, font=label_font, anchor=tk.W)
        
        for j in range(self.cols):
            # X轴坐标标签
            x_pos = j * self.grid_size + self.grid_size//2
            y_pos = self.rows * self.grid_size - 5
            self.canvas.create_text(x_pos, y_pos, 
                                  text=str(j), fill=axis_color, font=label_font, anchor=tk.N)
        
        # 添加轴标题
        self.canvas.create_text(self.cols * self.grid_size - 20, self.rows * self.grid_size - 20, 
                              text="X轴", fill=axis_color, font=("Arial", 12, "bold"))
        self.canvas.create_text(20, 20, 
                              text="Y轴", fill=axis_color, font=("Arial", 12, "bold"))
        
        # 获取当前选中的标签页
        current_tab = self.notebook.select()
        tab_index = self.notebook.index(current_tab) if current_tab else -1
        
        # 确定是否显示区域
        show_areas = self.show_all_areas
        
        # 强制显示高亮区域
        force_show_highlighted = (self.highlighted_area is not None)
        
        # 绘制标记区域（在单元格下方）
        if show_areas or force_show_highlighted:
            for area in self.marked_areas:
                # 如果不是显示所有区域模式，并且该区域不是高亮区域，则跳过
                if not show_areas and (not force_show_highlighted or area != self.highlighted_area):
                    continue
                
                # 如果区域没有单元格，跳过
                if not area.cells:
                    continue
                    
                # 确定区域颜色和透明度
                area_color = area.color
                outline_color = "gray"
                outline_width = 1
                
                # 如果是高亮区域，使用更明显的边框
                if self.highlighted_area is not None and area == self.highlighted_area:
                    outline_color = "black"
                    outline_width = 2
                
                # 绘制区域内的每个单元格
                for col, row in area.cells:
                    # 转换为画布坐标（原点在左下角）
                    canvas_y = self.rows * self.grid_size - (row+1) * self.grid_size
                    canvas_x = col * self.grid_size
                    
                    # 绘制单元格
                    self.canvas.create_rectangle(
                        canvas_x, canvas_y, 
                        canvas_x + self.grid_size, canvas_y + self.grid_size,
                        fill=area_color, outline=outline_color, width=outline_width,
                        stipple=""  # 实心填充
                    )
                    
                # 如果区域有单元格，在区域中心显示名称
                if area.cells:
                    center_x, center_y = area.get_center()
                    # 转换为画布坐标
                    canvas_y = self.rows * self.grid_size - (center_y+0.5) * self.grid_size
                    canvas_x = (center_x+0.5) * self.grid_size
                    
                    # 设置文本颜色
                    text_color = "black"
                    font_weight = "normal"
                    
                    # 如果是高亮区域，使用加粗字体
                    if self.highlighted_area is not None and area == self.highlighted_area:
                        font_weight = "bold"
                    
                    # 在区域中心显示名称
                    self.canvas.create_text(
                        canvas_x, canvas_y, 
                        text=area.name, 
                        fill=text_color, 
                        font=("Arial", 10, font_weight)
                    )
        
        # 绘制单元格
        for i in range(self.rows):
            for j in range(self.cols):
                # 转换为画布坐标（原点在左下角）
                canvas_y = self.rows * self.grid_size - (i+1) * self.grid_size
                canvas_x = j * self.grid_size
                
                x1, y1 = canvas_x, canvas_y
                x2, y2 = x1+self.grid_size, y1+self.grid_size
                
                # 颜色定义：空白(白)、障碍物(黑)、终点(红)
                color = "white"
                if self.grid[i][j] == 1:
                    color = "black"
                elif (i, j) == self.goal_pos:
                    color = "red"
                
                # 如果不是标记区域内的单元格，才绘制背景
                is_in_area = False
                if show_areas or force_show_highlighted:  # 只在显示区域模式下检查
                    for area in self.marked_areas:
                        # 如果不是显示所有区域模式，只检查高亮区域
                        if not show_areas and (not force_show_highlighted or area != self.highlighted_area):
                            continue
                            
                        if area.contains(j, i):  # 注意：contains接收(x,y)即(col,row)
                            is_in_area = True
                            break
                
                if not is_in_area:
                    # 绘制单元格背景
                    self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline="gray")
        
                # 如果是终点，在上面显示坐标信息
                if (i, j) == self.goal_pos:
                    # 计算单元格中心点
                    cx = x1 + self.grid_size // 2
                    cy = y1 + self.grid_size // 2
                    # 显示坐标值
                    coord_text = f"({j},{i})"
                    self.canvas.create_text(cx, cy, text=coord_text, 
                                          fill="white", font=("Arial", 10, "bold"))
        
        # 绘制当前位置和朝向
        if self.current_pos:
            row, col = self.current_pos
            # 转换为画布坐标（原点在左下角）
            canvas_y = self.rows * self.grid_size - (row+1) * self.grid_size
            canvas_x = col * self.grid_size
            
            # 绘制小车主体
            cx = canvas_x + self.grid_size // 2
            cy = canvas_y + self.grid_size // 2
            r = self.grid_size // 3
            self.canvas.create_oval(cx-r, cy-r, cx+r, cy+r, fill="blue", outline="black")
            
            # 使用计算得到的方向而不是小车上传的方向
            dir = self.calculated_direction
            
            # 绘制方向指示器
            dx, dy = 0, 0
            if dir == 0:  # 北
                dx, dy = 0, -1
            elif dir == 1:  # 东
                dx, dy = 1, 0
            elif dir == 2:  # 南
                dx, dy = 0, 1
            elif dir == 3:  # 西
                dx, dy = -1, 0
            
            # 箭头长度
            arrow_length = self.grid_size // 2
            self.canvas.create_line(
                cx, cy,
                cx + dx * arrow_length,
                cy + dy * arrow_length,
                arrow=tk.LAST, width=2, fill="yellow"
            )
            
            # 在小车位置显示坐标
            coord_text = f"({col},{row})"
            self.canvas.create_text(cx, cy - r - 10, text=coord_text, fill="blue")
        
        # 绘制路径
        if self.path:
            for i in range(len(self.path)-1):
                row1, col1 = self.path[i][0], self.path[i][1]  # 去掉方向，只取坐标
                row2, col2 = self.path[i+1][0], self.path[i+1][1]
                
                # 转换为画布坐标（原点在左下角）
                canvas_y1 = self.rows * self.grid_size - (row1+1) * self.grid_size + self.grid_size//2
                canvas_x1 = col1 * self.grid_size + self.grid_size//2
                canvas_y2 = self.rows * self.grid_size - (row2+1) * self.grid_size + self.grid_size//2
                canvas_x2 = col2 * self.grid_size + self.grid_size//2
                
                self.canvas.create_line(canvas_x1, canvas_y1, canvas_x2, canvas_y2, fill="purple", width=2)

        # 绘制基站
        for anchor in [self.anchor1, self.anchor2]:
            # 基站位置
            ax, ay = anchor.x, anchor.y
            
            # 转换为画布坐标（原点在左下角）
            canvas_y = self.rows * self.grid_size - (ay+1) * self.grid_size + self.grid_size//2
            canvas_x = ax * self.grid_size + self.grid_size//2
            
            # 绘制基站图标（正三角形）
            r = self.grid_size * 0.4  # 三角形大小
            
            # 计算三角形三个顶点（默认朝上的三角形）
            p1_x, p1_y = canvas_x, canvas_y - r  # 上顶点
            p2_x, p2_y = canvas_x - r * 0.8, canvas_y + r * 0.7  # 左下顶点
            p3_x, p3_y = canvas_x + r * 0.8, canvas_y + r * 0.7  # 右下顶点
            
            # 根据朝向旋转三角形
            angle_rad = math.radians(AnchorDirection.ANGLES[anchor.direction])
            
            # 旋转三角形顶点
            def rotate_point(px, py, angle_rad):
                # 以中心点旋转
                dx, dy = px - canvas_x, py - canvas_y
                new_x = canvas_x + dx * math.cos(angle_rad) - dy * math.sin(angle_rad)
                new_y = canvas_y + dx * math.sin(angle_rad) + dy * math.cos(angle_rad)
                return new_x, new_y
                
            p1_x, p1_y = rotate_point(p1_x, p1_y, angle_rad)
            p2_x, p2_y = rotate_point(p2_x, p2_y, angle_rad)
            p3_x, p3_y = rotate_point(p3_x, p3_y, angle_rad)
            
            # 绘制三角形
            anchor_color = "orange" if anchor == self.anchor1 else "purple"
            self.canvas.create_polygon(p1_x, p1_y, p2_x, p2_y, p3_x, p3_y, 
                                     fill=anchor_color, outline="black")
            
            # 显示基站ID和坐标
            id_text = f"ID:0x{anchor.anchor_id:X}"
            self.canvas.create_text(canvas_x, canvas_y - r - 10, text=id_text, fill=anchor_color, font=("Arial", 9))
            
            coord_text = f"({ax},{ay})"
            self.canvas.create_text(canvas_x, canvas_y + r + 10, text=coord_text, fill=anchor_color, font=("Arial", 9))

    def calculate_direction(self, current_pos, previous_pos):
        """根据连续两个位置计算朝向"""
        if not previous_pos or not current_pos:
            return self.calculated_direction  # 保持当前朝向
            
        # 注意：current_pos和previous_pos的格式是(row, col)，即(y, x)
        current_row, current_col = current_pos
        previous_row, previous_col = previous_pos
        
        # 如果位置没有变化，保持当前朝向
        if current_row == previous_row and current_col == previous_col:
            return self.calculated_direction
            
        # 计算移动方向 - 只使用四个基本方向
        # 0=北, 1=东, 2=南, 3=西
        # 注意：在网格中，row减小表示向北，row增加表示向南
        #      col增加表示向东，col减小表示向西
        
        # 计算行列变化
        row_change = current_row - previous_row
        col_change = current_col - previous_col
        
        print(f"位置变化: 从({previous_col},{previous_row})到({current_col},{current_row}), 行变化:{row_change}, 列变化:{col_change}")
        
        # 确定主要移动方向
        if abs(row_change) > abs(col_change):
            # 垂直移动为主
            if row_change < 0:
                # 行减小 = 向北
                return 0  # 北
            else:
                # 行增加 = 向南
                return 2  # 南
        else:
            # 水平移动为主
            if col_change > 0:
                # 列增加 = 向东
                return 1  # 东
            else:
                # 列减小 = 向西
                return 3  # 西

    def request_position(self):
        """请求小车当前位置"""
        if not self.bf_client.connected:
            self.status_var.set("未连接到巴法云")
            return
            
        self.status_var.set("正在请求小车位置...")
        success = self.bf_client.request_position()
        
        if success:
            self.waiting_for_response = True
            # 设置超时检查，15秒后检查是否收到响应（增加超时时间）
            self.root.after(15000, self.check_position_response)
            # 禁用获取位置按钮，直到收到响应或超时
            self.get_pos_btn.config(state=tk.DISABLED)
        else:
            self.status_var.set("位置请求失败，请尝试重连")
            
    def check_position_response(self):
        """检查位置响应是否超时"""
        if self.waiting_for_response and self.bf_client.waiting_for_position:
            self.status_var.set("位置请求超时，请检查单片机数据格式是否匹配")
            print("位置请求超时 - 可能的原因:")
            print("1. 单片机未收到请求或未正确处理position()命令")
            print("2. 单片机返回的数据格式与Python脚本期望的不匹配")
            print("3. UWB基站数据不可用或未连接")
            print("4. 网络连接不稳定")
            print("尝试重新连接...")
            self.waiting_for_response = False
            self.bf_client.waiting_for_position = False
            # 重新启用获取位置按钮
            self.get_pos_btn.config(state=tk.NORMAL)

    def update_position(self, data):
        """位置更新回调（主线程中更新UI）"""
        # 检查是否是导航命令
        if isinstance(data, dict) and "command" in data:
            self.root.after(0, lambda: self.process_area_navigate_command(data["command"]))
            return
        # 正常位置更新
        self.root.after(0, lambda: self._update_position_ui(data))

    def _update_position_ui(self, data):
        """更新UI显示的当前位置"""
        # 确定是标准位置元组还是双基站数据字典
        if isinstance(data, tuple) and len(data) == 3:
            # 标准位置元组格式
            x, y, reported_dir = data
            position_source = "单基站"
            
        elif isinstance(data, dict) and "position" in data:
            # 双基站数据字典格式
            x, y, reported_dir = data["position"]
            position_source = "双基站"
            
            # 更新基站数据
            if "anchor1" in data and data["anchor1"]:
                anchor1 = data["anchor1"]
                if self.anchor1.anchor_id == anchor1["id"]:
                    self.update_anchor_data(
                        anchor1["id"], 
                        anchor1["distance"], 
                        anchor1["azimuth"], 
                        anchor1["elevation"],
                        anchor1.get("grid_x", 0),
                        anchor1.get("grid_y", 0)
                    )
            
            if "anchor2" in data and data["anchor2"]:
                anchor2 = data["anchor2"]
                if self.anchor2.anchor_id == anchor2["id"]:
                    self.update_anchor_data(
                        anchor2["id"], 
                        anchor2["distance"], 
                        anchor2["azimuth"], 
                        anchor2["elevation"],
                        anchor2.get("grid_x", 0),
                        anchor2.get("grid_y", 0)
                    )
                    
            # 计算动态权重
            self.calculate_dynamic_weights()
                    
            # 使用基站数据计算绝对位置
            # 这里可以使用基站位置和朝向转换坐标
            # 如果基站配置了位置和朝向，使用convert_to_absolute方法计算
            if self.anchor1.data_ready and "anchor1" in data:
                # 设置基站数据
                self.anchor1.distance = data["anchor1"]["distance"]
                self.anchor1.azimuth = data["anchor1"]["azimuth"]
                
                # 计算绝对位置
                abs_x1, abs_y1 = self.anchor1.convert_to_absolute(self.grid_size)
                
                # 确保是整数
                abs_x1 = int(abs_x1) if abs_x1 is not None else 0
                abs_y1 = int(abs_y1) if abs_y1 is not None else 0
                
                # 如果第二个基站也有配置，使用权重计算
                if self.anchor2.data_ready and "anchor2" in data:
                    self.anchor2.distance = data["anchor2"]["distance"]
                    self.anchor2.azimuth = data["anchor2"]["azimuth"]
                    
                    abs_x2, abs_y2 = self.anchor2.convert_to_absolute(self.grid_size)
                    
                    # 确保是整数
                    abs_x2 = int(abs_x2) if abs_x2 is not None else 0
                    abs_y2 = int(abs_y2) if abs_y2 is not None else 0
                    
                    # 使用权重计算最终位置
                    if self.weight_mode == "fixed":
                        # 使用固定权重
                        x = int(abs_x1 * self.fixed_weight + abs_x2 * (1-self.fixed_weight))
                        y = int(abs_y1 * self.fixed_weight + abs_y2 * (1-self.fixed_weight))
                        position_source = f"基站计算(固定权重{int(self.fixed_weight*100)}:{int((1-self.fixed_weight)*100)})"
                    else:
                        # 使用动态权重
                        if self.active_anchor == self.anchor1:
                            x, y = abs_x1, abs_y1
                            position_source = f"基站1(距离:{self.anchor1.distance}cm)"
                        else:
                            x, y = abs_x2, abs_y2
                            position_source = f"基站2(距离:{self.anchor2.distance}cm)"
                else:
                    # 只用第一个基站
                    x, y = abs_x1, abs_y1
                    position_source = "基站1计算"
                
                # 确保坐标在有效范围内
                x = max(0, min(x, self.cols - 1))
                y = max(0, min(y, self.rows - 1))
                
                # 更新当前位置和方向
                self.current_pos = (y, x)  # 注意：画布坐标系是(y,x)
                print(f"计算出的绝对坐标: ({x}, {y})")
            else:
                # 如果基站数据不可用，使用原始位置数据
                print(f"使用原始位置数据: ({x}, {y})")
        else:
            # 未知数据格式
            print("收到无效位置数据格式")
            return
        
        # 更新当前位置和方向
        self.current_pos = (y, x)  # 注意：current_pos格式是(row, col)即(y, x)
        self.direction = reported_dir
        
        # 将当前位置设为起点
        self.start_pos = (y, x)
        
        # 计算方向（基于连续位置）
        if self.previous_pos:
            self.calculated_direction = self.calculate_direction(self.current_pos, self.previous_pos)
        else:
            self.calculated_direction = reported_dir  # 首次使用上报的朝向
        self.previous_pos = self.current_pos
        
        # 更新位置显示
        self.position_var.set(f"当前位置({position_source})：({x}, {y}) 方向：{self._direction_to_str(self.calculated_direction)}")
        
        # 如果已有终点，允许规划路径
        if self.goal_pos:
            self.plan_btn.config(state=tk.NORMAL)
            self.status_var.set("已获取位置，可以规划路径")
        
        # 重绘网格
        self.draw_grid()
        
        # 重要：如果收到位置更新，说明小车已经执行了上一个指令，重置等待状态
        self.waiting_for_response = False
        
        # 导航过程中，检查是否到达路径点
        if self.navigation_active and self.path:
            # 检查是否到达终点
            if self.is_at_position((y, x), (self.goal_pos[0], self.goal_pos[1])):
                self.status_var.set("已到达终点")
                self.navigation_active = False
                self.path = []
                
                # 发送TTS消息告知已到达目的地
                if self.current_area:
                    tts_message = f"我已经到达{self.current_area.name}，可以执行任务了。"
                    self.bf_client.send_tts_message(tts_message)
                    
                    # 发送到达通知到text2主题，通知控制AI可以执行后续任务
                    self.send_arrival_notification()
                return
                
            # 根据新位置重新规划路径
            self.replan_path()
            
            # 发送下一个路径点的指令
            self.root.after(1000, self.send_next_command)  # 延迟1秒发送下一个指令

    def replan_path(self):
        """根据当前位置重新规划路径"""
        if not self.start_pos or not self.goal_pos:
            return
            
        print(f"重新规划路径: 起点({self.start_pos[1]},{self.start_pos[0]}), 终点({self.goal_pos[1]},{self.goal_pos[0]})")
        print(f"路径规划使用四向控制: 北(0)、东(1)、南(2)、西(3)")
            
        grid_copy = [row[:] for row in self.grid]
        # 清除起点/终点标记（不视为障碍物）
        grid_copy[self.start_pos[0]][self.start_pos[1]] = 0
        grid_copy[self.goal_pos[0]][self.goal_pos[1]] = 0
        
        astar = AStar(grid_copy)
        path_coords = astar.get_path(self.start_pos, self.goal_pos)
        
        if not path_coords:
            print("无法找到有效路径")
            self.navigation_active = False
            self.status_var.set("无法找到路径")
            return
            
        # 为路径点添加方向信息
        self.path = []
        for i in range(len(path_coords)):
            row, col = path_coords[i]
            
            # 确定每个点的最优方向
            # 如果不是最后一个点，根据下一个点的位置确定方向
            if i < len(path_coords) - 1:
                next_row, next_col = path_coords[i+1]
                
                # 计算方向：0=北, 1=东, 2=南, 3=西
                # 注意：在网格中，row减小表示向北，row增加表示向南
                #      col增加表示向东，col减小表示向西
                row_change = next_row - row
                col_change = next_col - col
                
                if abs(row_change) > abs(col_change):
                    # 垂直移动为主
                    if row_change < 0:
                        # 行减小 = 向北
                        direction = 0  # 北
                    else:
                        # 行增加 = 向南
                        direction = 2  # 南
                else:
                    # 水平移动为主
                    if col_change > 0:
                        # 列增加 = 向东
                        direction = 1  # 东
                    else:
                        # 列减小 = 向西
                        direction = 3  # 西
                        
                print(f"路径点{i}->下一点: ({col},{row})->({next_col},{next_row}), 行变化:{row_change}, 列变化:{col_change}, 方向:{self._direction_to_str(direction)}")
            else:
                # 最后一个点的方向与前一个点相同
                if len(self.path) > 0:
                    direction = self.path[-1][2]
                else:
                    direction = self.calculated_direction
                    
            self.path.append((row, col, direction))
            
        print(f"路径重新规划完成，共{len(self.path)}个点")
        for i, point in enumerate(self.path[:5]):  # 只打印前5个点以避免过多输出
            print(f"路径点{i}: ({point[1]}, {point[0]}), 方向: {self._direction_to_str(point[2])}")
        if len(self.path) > 5:
            print(f"... 还有{len(self.path)-5}个点 ...")
            
        self.draw_grid()

    def send_next_command(self):
        """发送下一步指令（根据路径点和当前位置）"""
        if not self.path or not self.navigation_active:
            print("导航已停止或路径为空，不发送指令")
            return
            
        # 如果正在等待响应，不发送新指令
        if self.waiting_for_response:
            print("等待上一个指令响应，暂不发送新指令")
            print(f"当前等待状态: waiting_for_response={self.waiting_for_response}, bf_client.waiting_for_position={self.bf_client.waiting_for_position}")
            # 重新检查一次，如果等待时间过长，强制重置状态
            if hasattr(self, 'last_command_time') and time.time() - self.last_command_time > 10:
                print("等待时间过长，强制重置等待状态")
                self.waiting_for_response = False
                self.bf_client.waiting_for_position = False
            else:
                return
            
        # 确保当前位置已知
        if not self.current_pos:
            self.status_var.set("等待位置更新...")
            print("当前位置未知，尝试请求位置")
            # 尝试请求位置
            self.request_position()
            # 延迟2秒后重试
            self.root.after(2000, self.send_next_command)
            return
            
        current_row, current_col = self.current_pos
        print(f"当前位置: ({current_col}, {current_row}), 路径点数: {len(self.path)}")
        
        # 获取当前路径中的下一个点（跳过当前位置）
        next_index = -1
        for i, node in enumerate(self.path):
            node_row, node_col = node[0], node[1]
            if not self.is_at_position((node_row, node_col), (current_row, current_col)):
                next_index = i
                print(f"找到下一个路径点: {i}, 坐标({node_col}, {node_row})")
                break
                
        if next_index == -1:
            # 如果没有下一个点，说明已经到达终点
            self.status_var.set("已到达终点")
            self.navigation_active = False
            print("已到达终点或无法找到下一个路径点")
            return
        
        next_node = self.path[next_index]
        next_row, next_col, target_dir = next_node
        
        # 使用计算得到的朝向而不是小车上报的朝向
        current_dir = self.calculated_direction
        print(f"当前朝向: {self._direction_to_str(current_dir)}, 目标朝向: {self._direction_to_str(target_dir)}")
        
        # 确定命令
        command = ""
        
        # 计算下一个点相对于当前位置的方向
        # 注意：在网格中，row减小表示向北，row增加表示向南
        #      col增加表示向东，col减小表示向西
        row_change = next_row - current_row
        col_change = next_col - current_col
        
        print(f"下一点相对位置: ({current_col},{current_row})->({next_col},{next_row}), 行变化:{row_change}, 列变化:{col_change}")
        
        # 确定相对方向
        relative_dir = -1
        if abs(row_change) > abs(col_change):
            # 垂直移动为主
            if row_change < 0:
                # 行减小 = 向北
                relative_dir = 0  # 北
            else:
                # 行增加 = 向南
                relative_dir = 2  # 南
        else:
            # 水平移动为主
            if col_change > 0:
                # 列增加 = 向东
                relative_dir = 1  # 东
            else:
                # 列减小 = 向西
                relative_dir = 3  # 西
            
        print(f"相对方向: {self._direction_to_str(relative_dir) if relative_dir != -1 else '未知'}")
            
        # 计算需要转向的方向
        if relative_dir != -1 and relative_dir != current_dir:
            # 计算最短旋转方向
            diff = (relative_dir - current_dir) % 4
            print(f"需要转向: 当前{self._direction_to_str(current_dir)}->目标{self._direction_to_str(relative_dir)}, 差值:{diff}")
            if diff == 1:  # 向右转90度
                command = "right|move:1"  # 右转后前进一步
            elif diff == 2:  # 向后转180度
                command = "back|move:1"   # 掉头后前进一步
            elif diff == 3:  # 向左转90度
                command = "left|move:1"   # 左转后前进一步
            else:  # diff == 0，方向一致
                # 直线移动
                steps = self.calculate_straight_steps(current_row, current_col, relative_dir)
                command = f"move:{steps}"
        else:
            # 方向正确或无需转向，直接前进
            steps = self.calculate_straight_steps(current_row, current_col, relative_dir)
            command = f"move:{steps}"
        
        # 发送命令
        if command:
            print(f"发送指令: {command}")
            success = self.bf_client.send_command(command)
            if success:
                self.waiting_for_response = True
                self.last_command_time = time.time()  # 记录发送时间
                self.status_var.set(f"发送指令: {command}")
            else:
                self.status_var.set("指令发送失败")
                # 尝试重连
                self.root.after(3000, self.reconnect_and_retry)

    def calculate_straight_steps(self, current_row, current_col, direction):
        """计算在给定方向上可以直线前进的最大步数"""
        if direction == -1 or not self.path:
            return 1  # 默认前进一步
            
        max_steps = 1  # 至少前进一步
        
        # 遍历路径，找出同一直线上的连续点
        for node in self.path:
            node_row, node_col, _ = node
            
            # 检查是否在同一直线上
            is_same_line = False
            steps = 0
            
            # 注意：在网格中，row减小表示向北，row增加表示向南
            #      col增加表示向东，col减小表示向西
            if direction == 0:  # 北
                is_same_line = (node_col == current_col and node_row < current_row)
                steps = current_row - node_row
            elif direction == 1:  # 东
                is_same_line = (node_row == current_row and node_col > current_col)
                steps = node_col - current_col
            elif direction == 2:  # 南
                is_same_line = (node_col == current_col and node_row > current_row)
                steps = node_row - current_row
            elif direction == 3:  # 西
                is_same_line = (node_row == current_row and node_col < current_col)
                steps = current_col - node_col
                
            if is_same_line and steps > max_steps:
                max_steps = steps
                
        print(f"计算直线步数: 方向={self._direction_to_str(direction)}, 最大步数={max_steps}")
                
        # 限制一次移动的最大步数，避免过远移动
        return min(max_steps, 5)  # 最多一次移动5步

    def is_at_position(self, current, target, tolerance=1):
        """判断是否到达指定位置（允许±1误差）"""
        return abs(current[0] - target[0]) <= tolerance and abs(current[1] - target[1]) <= tolerance

    def _direction_to_str(self, dir_num):
        """将方向数值转为字符串"""
        directions = ["北", "东", "南", "西"]
        if 0 <= dir_num < len(directions):
            return directions[dir_num]
        return "未知"

    def plan_path(self):
        """使用A*算法规划路径"""
        if not self.start_pos or not self.goal_pos:
            messagebox.showerror("错误", "请先设置起点和终点")
            return
            
        print(f"开始规划路径: 起点({self.start_pos[1]},{self.start_pos[0]}), 终点({self.goal_pos[1]},{self.goal_pos[0]})")
        print(f"路径规划规则: 尽量保持与墙体至少{AStar(self.grid).wall_safety_distance}格距离，除非是狭窄通道")
        print(f"路径规划使用四向控制: 北(0)、东(1)、南(2)、西(3)")
            
        grid_copy = [row[:] for row in self.grid]
        # 清除起点/终点标记（不视为障碍物）
        grid_copy[self.start_pos[0]][self.start_pos[1]] = 0
        grid_copy[self.goal_pos[0]][self.goal_pos[1]] = 0
        
        astar = AStar(grid_copy)
        path_coords = astar.get_path(self.start_pos, self.goal_pos)
        
        if not path_coords:
            messagebox.showerror("错误", "无有效路径")
            print("无法找到有效路径")
            return
            
        print(f"找到路径，共{len(path_coords)}个点")
        for i, point in enumerate(path_coords):
            print(f"路径点{i}: ({point[1]}, {point[0]})")
            
        # 为路径点添加方向信息
        self.path = []
        for i in range(len(path_coords)):
            row, col = path_coords[i]
            
            # 确定每个点的最优方向
            # 如果不是最后一个点，根据下一个点的位置确定方向
            if i < len(path_coords) - 1:
                next_row, next_col = path_coords[i+1]
                
                # 计算方向：0=北, 1=东, 2=南, 3=西
                # 注意：在网格中，row减小表示向北，row增加表示向南
                #      col增加表示向东，col减小表示向西
                row_change = next_row - row
                col_change = next_col - col
                
                if abs(row_change) > abs(col_change):
                    # 垂直移动为主
                    if row_change < 0:
                        # 行减小 = 向北
                        direction = 0  # 北
                    else:
                        # 行增加 = 向南
                        direction = 2  # 南
                else:
                    # 水平移动为主
                    if col_change > 0:
                        # 列增加 = 向东
                        direction = 1  # 东
                    else:
                        # 列减小 = 向西
                        direction = 3  # 西
                        
                print(f"路径点{i}->下一点: ({col},{row})->({next_col},{next_row}), 行变化:{row_change}, 列变化:{col_change}, 方向:{self._direction_to_str(direction)}")
            else:
                # 最后一个点的方向与前一个点相同
                if len(self.path) > 0:
                    direction = self.path[-1][2]
                else:
                    direction = self.calculated_direction
                    
            self.path.append((row, col, direction))
            
            self.status_var.set(f"路径规划完成，共{len(self.path)}个点")
            self.draw_grid()
            self.start_btn.config(state=tk.NORMAL)

    def start_navigation(self):
        """开始导航"""
        if not self.path:
            messagebox.showerror("错误", "请先规划有效路径")
            return
            
        self.navigation_active = True
        self.waiting_for_response = False
        self.status_var.set("开始导航")
        self.start_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        
        # 发送第一个路径点指令
        self.send_next_command()

    def stop_navigation(self):
        """停止导航"""
        self.navigation_active = False
        self.waiting_for_response = False
        self.status_var.set("导航已停止")
        self.start_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        
        # 发送停止指令
        self.bf_client.send_command("STOP")

    def reconnect_and_retry(self):
        """重新连接并重试"""
        if self.bf_client.reconnect():
            self.status_var.set("已重新连接到巴法云")
            self.get_pos_btn.config(state=tk.NORMAL)
            self.reconnect_btn.config(state=tk.NORMAL)
            self.send_next_command()
        else:
            self.status_var.set("重连失败，请检查网络")
            
    # === 基站配置相关方法 ===
    def toggle_anchor_mode(self):
        """切换基站配置模式"""
        self.anchor_config_mode = self.anchor_mode_var.get()
        if self.anchor_config_mode:
            self.status_var.set("基站配置模式：请在画布上点击选择基站位置")
            # 禁用障碍物模式
            self.obstacle_mode_var.set(False)
        else:
            self.status_var.set("基站配置模式已关闭")
            self.selected_anchor = None
        self.draw_grid()
        
    def select_anchor(self, anchor):
        """选择要配置的基站"""
        self.selected_anchor = anchor
        self.anchor_mode_var.set(True)
        self.anchor_config_mode = True
        self.status_var.set(f"请在画布上点击设置基站ID:0x{anchor.anchor_id:X}的位置")
        print(f"选择基站ID:0x{anchor.anchor_id:X}进行配置，当前位置:({anchor.x},{anchor.y})，朝向:{AnchorDirection.NAMES[anchor.direction]}")
        print("请在画布上点击设置基站位置，位置设置对坐标转换至关重要！")
        
        # 在UI上高亮显示选中的基站
        if anchor == self.anchor1:
            self.select_anchor1_btn.config(relief=tk.SUNKEN, bg="lightblue")
            self.select_anchor2_btn.config(relief=tk.RAISED, bg="SystemButtonFace")
        else:
            self.select_anchor1_btn.config(relief=tk.RAISED, bg="SystemButtonFace")
            self.select_anchor2_btn.config(relief=tk.SUNKEN, bg="lightblue")
        
    def change_anchor_direction(self, anchor, direction_name):
        """修改基站朝向"""
        # 从方向名称转换为方向值
        for i, name in enumerate(AnchorDirection.NAMES):
            if name == direction_name:
                anchor.direction = i
                break
        # 更新界面
        self.draw_grid()
        
        # 更新位置标签
        if anchor == self.anchor1:
            self.anchor1_dir_var.set(direction_name)
        else:
            self.anchor2_dir_var.set(direction_name)
            
    def update_anchor_data(self, anchor_id, distance, azimuth, elevation, grid_x, grid_y):
        """更新基站测量数据"""
        # 找到对应基站
        anchor = None
        if anchor_id == self.anchor1.anchor_id:
            anchor = self.anchor1
        elif anchor_id == self.anchor2.anchor_id:
            anchor = self.anchor2
        else:
            return False
            
        # 更新基站数据
        anchor.distance = distance
        anchor.azimuth = azimuth
        anchor.elevation = elevation
        anchor.grid_x = grid_x
        anchor.grid_y = grid_y
        anchor.last_update_time = time.time()
        anchor.data_ready = True  # 确保设置数据就绪标志
        
        print(f"更新基站ID:0x{anchor_id:X}数据: 距离={distance}cm, 角度={azimuth}°, 网格:({grid_x},{grid_y})")
        
        # 更新基站数据显示
        self.update_anchor_data_display()
        
        # 重绘网格
        self.draw_grid()
        return True
        
    def update_anchor_data_display(self):
        """更新基站数据显示区域"""
        self.anchor_data_text.config(state=tk.NORMAL)
        self.anchor_data_text.delete(1.0, tk.END)
        
        # 显示基站1数据
        self.anchor_data_text.insert(tk.END, f"基站1 (ID:0x{self.anchor1.anchor_id:X}):\n")
        self.anchor_data_text.insert(tk.END, f"  位置: ({self.anchor1.x}, {self.anchor1.y})\n")
        self.anchor_data_text.insert(tk.END, f"  朝向: {AnchorDirection.NAMES[self.anchor1.direction]}\n")
        self.anchor_data_text.insert(tk.END, f"  距离: {self.anchor1.distance}cm, 方位角: {self.anchor1.azimuth}°\n")
        self.anchor_data_text.insert(tk.END, f"  权重: {self.anchor1.weight:.2f}\n")
        
        # 显示基站2数据
        self.anchor_data_text.insert(tk.END, f"基站2 (ID:0x{self.anchor2.anchor_id:X}):\n")
        self.anchor_data_text.insert(tk.END, f"  位置: ({self.anchor2.x}, {self.anchor2.y})\n")
        self.anchor_data_text.insert(tk.END, f"  朝向: {AnchorDirection.NAMES[self.anchor2.direction]}\n")
        self.anchor_data_text.insert(tk.END, f"  距离: {self.anchor2.distance}cm, 方位角: {self.anchor2.azimuth}°\n")
        self.anchor_data_text.insert(tk.END, f"  权重: {self.anchor2.weight:.2f}\n")
        
        # 显示当前活跃基站
        if self.weight_mode == "dynamic":
            active_id = self.active_anchor.anchor_id if self.active_anchor else 0
            self.anchor_data_text.insert(tk.END, f"当前主导基站: 0x{active_id:X}\n")
        
        # 显示合成位置
        if self.current_pos:
            self.anchor_data_text.insert(tk.END, f"小车位置: ({self.current_pos[0]}, {self.current_pos[1]}), 朝向: {self._direction_to_str(self.calculated_direction)}\n")
        
        self.anchor_data_text.config(state=tk.DISABLED)

    # 添加权重相关方法
    def change_weight_mode(self, mode):
        """切换权重模式"""
        self.weight_mode = mode
        if mode == "fixed":
            self.weight_scale.config(state=tk.NORMAL)
            self.status_var.set("已切换到固定权重模式")
        else:
            self.weight_scale.config(state=tk.DISABLED)
            self.status_var.set("已切换到动态权重模式")
            
    def update_weight_value(self, value):
        """更新固定权重值"""
        weight = int(value) / 100.0
        self.fixed_weight = weight
        self.weight_label.config(text=f"{int(weight * 100)}:{int((1-weight) * 100)}")
        
        # 更新基站权重
        if self.weight_mode == "fixed":
            self.anchor1.weight = self.fixed_weight
            self.anchor2.weight = 1.0 - self.fixed_weight
            self.update_anchor_data_display()
            
    def calculate_dynamic_weights(self):
        """计算动态权重"""
        if not self.anchor1.data_ready or not self.anchor2.data_ready:
            # 如果某个基站数据不可用，使用固定权重
            return
            
        # 基于距离计算权重
        dist1 = self.anchor1.distance
        dist2 = self.anchor2.distance
        
        # 避免除零错误
        if dist1 <= 0:
            dist1 = 1
        if dist2 <= 0:
            dist2 = 1
            
        # 计算权重 - 距离越近权重越大
        total_inv_dist = (1.0/dist1) + (1.0/dist2)
        weight1 = (1.0/dist1) / total_inv_dist
        weight2 = (1.0/dist2) / total_inv_dist
        
        # 应用权重
        self.anchor1.weight = weight1
        self.anchor2.weight = weight2
        
        # 确定主导基站
        if dist1 <= dist2:
            self.active_anchor = self.anchor1
        else:
            self.active_anchor = self.anchor2
            
        # 如果使用极端权重模式(100:0)
        if self.weight_mode == "dynamic":
            self.anchor1.weight = 1.0 if self.active_anchor == self.anchor1 else 0.0
            self.anchor2.weight = 1.0 if self.active_anchor == self.anchor2 else 0.0
            
        # 更新显示
        self.update_anchor_data_display()

    def toggle_marking_mode(self):
        """切换标记区域模式"""
        self.marking_mode = self.marking_mode_var.get()
        if self.marking_mode:
            self.status_var.set("标记区域模式：请点击网格添加/移除单元格")
            # 禁用其他模式
            self.obstacle_mode_var.set(False)
            self.anchor_mode_var.set(False)
            self.anchor_config_mode = False
            
            # 确保在标记模式下区域可见
            self.show_all_areas = True
            self.show_all_areas_var.set(True)
            
            # 如果没有当前区域，提示创建
            if self.current_area is None:
                messagebox.showinfo("提示", "请先创建一个新区域")
                self.create_new_area()
        else:
            self.status_var.set("标记区域模式已关闭")
            
            # 根据当前标签页决定是否显示区域
            current_tab = self.notebook.index(self.notebook.select()) if hasattr(self, 'notebook') else -1
            if current_tab != 2:  # 如果不在标记区域标签页
                self.show_all_areas = False
                self.show_all_areas_var.set(False)
            
        # 重绘网格以反映变化
        self.draw_grid()

    def create_new_area(self):
        """创建新的标记区域"""
        # 弹出对话框，让用户输入区域名称
        area_name = simpledialog.askstring("创建新区域", "请输入区域名称:", parent=self.root)
        if area_name:
            # 创建新的标记区域
            new_area = MarkedArea(area_name, self.color_var.get())
            
            # 添加到区域列表
            self.marked_areas.append(new_area)
            
            # 设置为当前区域
            self.current_area = new_area
            self.current_area_var.set(f"当前区域: {area_name}")
            
            # 高亮显示新区域
            self.highlighted_area = new_area
            
            # 确保区域在创建时可见
            current_tab = self.notebook.index(self.notebook.select()) if hasattr(self, 'notebook') else -1
            if current_tab == 2:  # 如果当前在标记区域标签页
                self.show_all_areas = True
                self.show_all_areas_var.set(True)
            
            # 更新区域列表显示
            self.update_area_listbox()
            
            # 选中新创建的区域
            index = len(self.marked_areas) - 1
            if hasattr(self, 'area_listbox'):
                self.area_listbox.selection_clear(0, tk.END)
                self.area_listbox.selection_set(index)
            
            # 自动开启标记模式
            self.marking_mode_var.set(True)
            self.marking_mode = True
            
            self.status_var.set(f"已创建新区域: {area_name}，请点击网格添加单元格")
            
            # 重绘网格
            self.draw_grid()
        else:
            # 如果用户取消输入，关闭标记模式
            self.marking_mode_var.set(False)
            self.marking_mode = False
            
    def on_canvas_drag(self, event):
        """拖动鼠标绘制标记区域"""
        if not self.marking_mode:
            return
        
        # 如果是第一次点击，记录起点
        if self.marking_start is None:
            canvas_x = event.x
            canvas_y = event.y
            
            # 转换为网格坐标
            col = canvas_x // self.grid_size
            # 反转y轴，使原点在左下角
            row = self.rows - 1 - (canvas_y // self.grid_size)
            
            self.marking_start = (col, row)
            
        # 更新当前点
        canvas_x = event.x
        canvas_y = event.y
        
        # 转换为网格坐标
        col = canvas_x // self.grid_size
        # 反转y轴，使原点在左下角
        row = self.rows - 1 - (canvas_y // self.grid_size)
        
        self.marking_current = (col, row)
        
        # 重绘网格显示标记区域
        self.draw_grid()
        
    def on_canvas_release(self, event):
        """释放鼠标完成标记区域"""
        # 在新的标记方式中，不再需要处理鼠标释放事件
        # 因为我们是通过单击添加/删除单元格，而不是拖动选择区域
        pass
        
    def on_canvas_motion(self, event):
        """鼠标移动时显示区域信息"""
        if not self.canvas:
            return
            
        # 获取鼠标位置
        canvas_x = event.x
        canvas_y = event.y
        
        # 转换为网格坐标
        col = canvas_x // self.grid_size
        # 反转y轴，使原点在左下角
        row = self.rows - 1 - (canvas_y // self.grid_size)
        
        # 检查是否在有效范围内
        if 0 <= col < self.cols and 0 <= row < self.rows:
            # 更新鼠标位置显示
            self.hover_info_var.set(f"鼠标位置: ({col}, {row})")
            
            # 检查是否在某个标记区域内
            for area in self.marked_areas:
                if area.contains(col, row):
                    # 更新悬停信息
                    center_x, center_y = area.get_center()
                    bounds = area.get_bounds()
                    self.hover_info = f"区域: {area.name} 中心:({center_x},{center_y}) 单元格数:{len(area.cells)}"
                    self.hover_info_var.set(self.hover_info)
                    return
                    
            # 如果不在任何区域内，清除悬停信息
            self.hover_info = None
        else:
            # 超出范围
            self.hover_info_var.set("鼠标位置: 超出范围")
            self.hover_info = None
            
    def update_area_listbox(self):
        """更新区域列表显示"""
        # 清空列表
        self.area_listbox.delete(0, tk.END)
        
        # 添加所有区域
        for area in self.marked_areas:
            self.area_listbox.insert(tk.END, area.name)
        
        print(f"更新区域列表，共{len(self.marked_areas)}个区域")
        
    def on_area_select(self, event):
        """选择区域时显示详细信息并设为当前区域"""
        # 获取选中的索引
        selected_indices = self.area_listbox.curselection()
        if not selected_indices:
            self.highlighted_area = None
            self.current_area = None
            self.current_area_var.set("当前区域: 无")
            self.area_info_var.set("区域信息: 未选择")
            self.draw_grid()
            return
            
        index = selected_indices[0]
        if 0 <= index < len(self.marked_areas):
            area = self.marked_areas[index]
            
            # 如果区域没有单元格，显示警告
            if not area.cells:
                print(f"警告: 区域 '{area.name}' 没有单元格")
                self.area_info_var.set(f"区域信息: {area.name} - 警告: 没有单元格")
            else:
                center_x, center_y = area.get_center()
                self.area_info_var.set(f"区域信息: {area.name} 中心点:({center_x},{center_y}) 单元格数:{len(area.cells)}")
            
            # 设置为当前区域
            self.current_area = area
            self.current_area_var.set(f"当前区域: {area.name}")
            
            # 高亮显示选中的区域
            self.highlighted_area = area
            
            print(f"选中区域: {area.name}, 单元格数: {len(area.cells)}, 中心点: {area.get_center()}")
            
            # 重绘网格
            self.draw_grid()

    def rename_area(self):
        """重命名选中的区域"""
        # 获取选中的索引
        selected_indices = self.area_listbox.curselection()
        if not selected_indices:
            messagebox.showinfo("提示", "请先选择一个区域")
            return
            
        index = selected_indices[0]
        if 0 <= index < len(self.marked_areas):
            area = self.marked_areas[index]
            
            # 弹出对话框，让用户输入新名称
            new_name = simpledialog.askstring("重命名区域", 
                                           f"请输入新名称 (当前: {area.name}):", 
                                           parent=self.root)
            if new_name:
                area.name = new_name
                
                # 更新区域列表显示
                self.update_area_listbox()
                
                # 更新信息显示
                center_x, center_y = area.get_center()
                self.area_info_var.set(f"区域信息: {area.name} 中心点:({center_x},{center_y}) 单元格数:{len(area.cells)}")
                
                # 如果是当前区域，更新当前区域显示
                if self.current_area == area:
                    self.current_area_var.set(f"当前区域: {new_name}")
                
                self.status_var.set(f"区域已重命名为: {new_name}")
                
                # 重绘网格以更新区域名称显示
                self.draw_grid()
                
    def delete_area(self):
        """删除选中的区域"""
        # 获取选中的索引
        selected_indices = self.area_listbox.curselection()
        if not selected_indices:
            messagebox.showinfo("提示", "请先选择一个区域")
            return
            
        index = selected_indices[0]
        if 0 <= index < len(self.marked_areas):
            area = self.marked_areas[index]
            
            # 确认删除
            if messagebox.askyesno("确认删除", f"确定要删除区域 '{area.name}' 吗?"):
                # 如果删除的是当前区域，清除当前区域
                if self.current_area == area:
                    self.current_area = None
                    self.current_area_var.set("当前区域: 无")
                    
                    # 如果正在标记模式，关闭标记模式
                    if self.marking_mode:
                        self.marking_mode_var.set(False)
                        self.marking_mode = False
                
                # 删除区域
                del self.marked_areas[index]
                
                # 更新区域列表显示
                self.update_area_listbox()
                
                # 更新信息显示
                self.area_info_var.set("区域信息: 未选择")
                
                self.status_var.set(f"区域已删除: {area.name}")
                
                # 重绘网格
                self.draw_grid()
                
    def navigate_to_area(self):
        """导航到选中的区域中心点"""
        # 获取选中的索引
        selected_indices = self.area_listbox.curselection()
        if not selected_indices:
            messagebox.showinfo("提示", "请先选择一个区域")
            return
            
        index = selected_indices[0]
        if 0 <= index < len(self.marked_areas):
            area = self.marked_areas[index]
            
            # 检查区域是否有单元格
            if not area.cells:
                messagebox.showinfo("提示", f"区域 '{area.name}' 没有单元格，无法导航")
                return
            
            # 获取区域中心点
            center_x, center_y = area.get_center()
            
            # 设置为导航终点
            self.goal_pos = (center_y, center_x)  # 注意：goal_pos格式是(row, col)
            
            # 更新终点显示
            self.goal_pos_var.set(f"终点位置：({center_x}, {center_y})")
            
            # 如果已有起点，允许规划路径
            if self.start_pos:
                self.plan_btn.config(state=tk.NORMAL)
                
            self.status_var.set(f"已设置导航目标为区域中心: {area.name} ({center_x},{center_y})")
            
            # 重绘网格
            self.draw_grid()

    def save_config(self):
        """保存配置到JSON文件"""
        try:
            # 准备要保存的区域数据
            area_data = []
            for area in self.marked_areas:
                area_dict = area.to_dict()
                area_data.append(area_dict)
                print(f"保存区域: {area.name}, 单元格数: {len(area.cells)}, 中心点: {area.get_center()}")
            
            config = {
                # 保存基站配置
                "anchors": [
                    self.anchor1.to_dict(),
                    self.anchor2.to_dict()
                ],
                # 保存标记区域
                "marked_areas": area_data,
                # 保存障碍物
                "obstacles": [],
                # 保存UI状态
                "ui_state": {
                    "show_all_areas": self.show_all_areas,
                    "highlighted_area_index": -1,  # 默认值
                    "current_tab": self.notebook.index(self.notebook.select()) if hasattr(self, 'notebook') else 0
                }
            }
            
            # 保存当前选中的区域索引
            if self.highlighted_area is not None and self.highlighted_area in self.marked_areas:
                highlighted_index = self.marked_areas.index(self.highlighted_area)
                config["ui_state"]["highlighted_area_index"] = highlighted_index
                print(f"保存选中区域索引: {highlighted_index}, 区域名: {self.highlighted_area.name}")
            
            # 保存障碍物位置
            obstacle_count = 0
            for i in range(self.rows):
                for j in range(self.cols):
                    if self.grid[i][j] == 1:  # 如果是障碍物
                        config["obstacles"].append({"row": i, "col": j})
                        obstacle_count += 1
            
            print(f"保存配置: {len(area_data)}个区域, {obstacle_count}个障碍物, 基站1位置({self.anchor1.x},{self.anchor1.y}), 基站2位置({self.anchor2.x},{self.anchor2.y})")
            
            # 写入文件
            with open(self.config_file, "w") as f:
                json.dump(config, f, indent=2)
                
            self.status_var.set(f"配置已保存到: {self.config_file}")
            messagebox.showinfo("保存成功", f"配置已保存到: {self.config_file}")
        except Exception as e:
            self.status_var.set(f"保存配置失败: {e}")
            messagebox.showerror("保存失败", f"保存配置失败: {e}")
            import traceback
            traceback.print_exc()

    def load_config(self):
        """从JSON文件加载配置"""
        try:
            # 检查文件是否存在
            if not os.path.exists(self.config_file):
                print(f"配置文件不存在: {self.config_file}")
                return False
                
            # 读取文件
            with open(self.config_file, "r") as f:
                config = json.load(f)
                
            print(f"正在加载配置文件: {self.config_file}")
                
            # 加载基站配置
            if "anchors" in config and len(config["anchors"]) >= 2:
                try:
                    # 加载基站1
                    anchor1_data = config["anchors"][0]
                    self.anchor1 = AnchorConfig.from_dict(anchor1_data)
                    if hasattr(self, 'anchor1_pos_label'):
                        self.anchor1_pos_label.config(text=f"({self.anchor1.x},{self.anchor1.y})")
                    if hasattr(self, 'anchor1_dir_var'):
                        self.anchor1_dir_var.set(AnchorDirection.NAMES[self.anchor1.direction])
                    
                    # 加载基站2
                    anchor2_data = config["anchors"][1]
                    self.anchor2 = AnchorConfig.from_dict(anchor2_data)
                    if hasattr(self, 'anchor2_pos_label'):
                        self.anchor2_pos_label.config(text=f"({self.anchor2.x},{self.anchor2.y})")
                    if hasattr(self, 'anchor2_dir_var'):
                        self.anchor2_dir_var.set(AnchorDirection.NAMES[self.anchor2.direction])
                    
                    # 更新基站列表
                    self.anchors = [self.anchor1, self.anchor2]
                    print(f"加载基站配置: 基站1位置({self.anchor1.x},{self.anchor1.y}), 基站2位置({self.anchor2.x},{self.anchor2.y})")
                except Exception as e:
                    print(f"加载基站配置失败: {e}")
                
            # 清空现有标记区域
            self.marked_areas = []
            self.current_area = None
            self.highlighted_area = None
            if hasattr(self, 'current_area_var'):
                self.current_area_var.set("当前区域: 无")
            if hasattr(self, 'area_info_var'):
                self.area_info_var.set("区域信息: 未选择")
            
            print("已清空现有标记区域")
            
            # 加载标记区域
            if "marked_areas" in config:
                try:
                    # 从配置文件重新生成区域
                    for area_data in config["marked_areas"]:
                        new_area = MarkedArea.from_dict(area_data)
                        
                        # 检查区域是否有单元格
                        if not new_area.cells:
                            print(f"警告: 区域 '{new_area.name}' 没有单元格，跳过加载")
                            continue
                            
                        self.marked_areas.append(new_area)
                        print(f"加载区域: {new_area.name}, 单元格数: {len(new_area.cells)}, 中心点: {new_area.get_center()}")
                    
                    print(f"成功加载 {len(self.marked_areas)} 个标记区域")
                    
                    # 更新区域列表显示
                    if hasattr(self, 'area_listbox'):
                        self.update_area_listbox()
                        
                    # 恢复UI状态
                    if "ui_state" in config:
                        ui_state = config["ui_state"]
                        
                        # 根据当前标签页设置区域显示状态
                        current_tab = self.notebook.index(self.notebook.select()) if hasattr(self, 'notebook') else -1
                        if current_tab == 2:  # 如果当前是标记区域标签页
                            # 标记区域标签页中显示区域
                            self.show_all_areas = True 
                            if hasattr(self, 'show_all_areas_var'):
                                self.show_all_areas_var.set(True)
                        else:
                            # 非标记区域标签页中不显示区域
                            self.show_all_areas = False
                            if hasattr(self, 'show_all_areas_var'):
                                self.show_all_areas_var.set(False)
                        
                        # 恢复选中的区域
                        highlighted_area_index = ui_state.get("highlighted_area_index", -1)
                        if 0 <= highlighted_area_index < len(self.marked_areas):
                            self.highlighted_area = self.marked_areas[highlighted_area_index]
                            self.current_area = self.marked_areas[highlighted_area_index]
                            
                            # 更新UI显示
                            if hasattr(self, 'area_listbox'):
                                self.area_listbox.selection_clear(0, tk.END)
                                self.area_listbox.selection_set(highlighted_area_index)
                                
                            if hasattr(self, 'current_area_var'):
                                self.current_area_var.set(f"当前区域: {self.current_area.name}")
                                
                            # 更新区域信息
                            center_x, center_y = self.current_area.get_center()
                            if hasattr(self, 'area_info_var'):
                                self.area_info_var.set(f"区域信息: {self.current_area.name} 中心点:({center_x},{center_y}) 单元格数:{len(self.current_area.cells)}")
                            
                            print(f"选中区域: {self.current_area.name}, 索引: {highlighted_area_index}")
                        
                        # 尝试切换到保存的标签页
                        saved_tab_index = ui_state.get("current_tab", 0)
                        if hasattr(self, 'notebook') and 0 <= saved_tab_index < self.notebook.index('end'):
                            try:
                                self.notebook.select(saved_tab_index)
                                print(f"切换到标签页: {saved_tab_index}")
                                
                                # 根据切换后的标签页设置区域显示状态
                                if saved_tab_index == 2:  # 如果是标记区域标签页
                                    self.show_all_areas = True
                                    if hasattr(self, 'show_all_areas_var'):
                                        self.show_all_areas_var.set(True)
                                else:
                                    self.show_all_areas = False
                                    if hasattr(self, 'show_all_areas_var'):
                                        self.show_all_areas_var.set(False)
                            except Exception as e:
                                print(f"切换标签页失败: {e}")
                    else:
                        # 如果没有UI状态信息，根据当前标签页设置区域显示状态
                        current_tab = self.notebook.index(self.notebook.select()) if hasattr(self, 'notebook') else -1
                        if current_tab == 2:  # 如果当前是标记区域标签页
                            self.show_all_areas = True
                            if hasattr(self, 'show_all_areas_var'):
                                self.show_all_areas_var.set(True)
                        else:
                            self.show_all_areas = False
                            if hasattr(self, 'show_all_areas_var'):
                                self.show_all_areas_var.set(False)
                            
                        # 如果有区域，选择第一个作为当前区域
                        if self.marked_areas and hasattr(self, 'area_listbox'):
                            self.area_listbox.selection_clear(0, tk.END)
                            self.area_listbox.selection_set(0)
                            self.current_area = self.marked_areas[0]
                            self.highlighted_area = self.marked_areas[0]  # 确保高亮区域被设置
                            if hasattr(self, 'current_area_var'):
                                self.current_area_var.set(f"当前区域: {self.current_area.name}")
                            print(f"默认选中第一个区域: {self.current_area.name}")
                        
                except Exception as e:
                    print(f"加载标记区域失败: {e}")
                    import traceback
                    traceback.print_exc()
                
            # 加载障碍物
            if "obstacles" in config:
                try:
                    # 清空网格
                    self.grid = [[0 for _ in range(self.cols)] for _ in range(self.rows)]
                    # 设置障碍物
                    obstacle_count = 0
                    for obstacle in config["obstacles"]:
                        row = obstacle["row"]
                        col = obstacle["col"]
                        if 0 <= row < self.rows and 0 <= col < self.cols:
                            self.grid[row][col] = 1
                            obstacle_count += 1
                    print(f"加载障碍物: {obstacle_count}个")
                except Exception as e:
                    print(f"加载障碍物失败: {e}")
            
            if hasattr(self, 'status_var'):
                self.status_var.set(f"配置已从 {self.config_file} 加载")
            print(f"配置已从 {self.config_file} 加载")
            
            # 重绘网格
            self.draw_grid()
            return True
        except Exception as e:
            if hasattr(self, 'status_var'):
                self.status_var.set(f"加载配置失败: {e}")
            print(f"加载配置失败: {e}")
            import traceback
            traceback.print_exc()
            return False

    def on_tab_changed(self, event):
        """处理标签页切换事件"""
        # 获取当前选中的标签页
        current_tab = self.notebook.select()
        tab_index = self.notebook.index(current_tab)
        
        # 根据标签页切换显示模式
        if tab_index == 2:  # 标记区域标签页
            # 在标记区域标签页，根据复选框状态显示区域
            self.show_all_areas = self.show_all_areas_var.get()
            
            # 如果有选中的区域，确保它可见
            if self.highlighted_area:
                # 强制显示区域，确保可见性
                self.show_all_areas = True
                self.show_all_areas_var.set(True)
            
            if self.marked_areas:
                # 如果有区域但没有选中的区域，自动选择第一个
                if not self.highlighted_area:
                    self.area_listbox.selection_clear(0, tk.END)
                    self.area_listbox.selection_set(0)
                    self.highlighted_area = self.marked_areas[0]
                    self.current_area = self.marked_areas[0]
                    self.current_area_var.set(f"当前区域: {self.current_area.name}")
                    center_x, center_y = self.current_area.get_center()
                    self.area_info_var.set(f"区域信息: {self.current_area.name} 中心点:({center_x},{center_y}) 单元格数:{len(self.current_area.cells)}")
        else:
            # 非标记区域标签页，默认隐藏所有区域
            self.show_all_areas = False
            if hasattr(self, 'show_all_areas_var'):
                self.show_all_areas_var.set(False)
        
        # 重绘画布
        self.draw_grid()

    def navigate_to_area_by_name(self, area_name):
        """根据区域名称导航到指定区域"""
        # 查找匹配的区域
        target_area = None
        for area in self.marked_areas:
            if area.name.lower() == area_name.lower():
                target_area = area
                break
                
        if not target_area:
            # 尝试模糊匹配
            for area in self.marked_areas:
                if area_name.lower() in area.name.lower() or area.name.lower() in area_name.lower():
                    target_area = area
                    break
                    
        if target_area:
            # 找到匹配的区域，设置为当前高亮区域
            self.highlighted_area = target_area
            self.current_area = target_area
            
            # 更新UI
            self.current_area_var.set(f"当前区域: {target_area.name}")
            
            # 获取区域中心点
            center_x, center_y = target_area.get_center()
            
            # 设置为导航终点
            self.goal_pos = (center_y, center_x)  # 注意：goal_pos格式是(row, col)
            
            # 更新终点显示
            self.goal_pos_var.set(f"终点位置：({center_x}, {center_y})")
            
            # 如果已有起点，规划路径
            if self.start_pos:
                self.plan_path()
                # 开始导航
                self.start_navigation()
                
                # 发送TTS消息告知小车正在前往目的地
                tts_message = f"我正在前往{target_area.name}，请稍等。"
                self.bf_client.send_tts_message(tts_message)
                
                self.status_var.set(f"正在导航到区域: {target_area.name}")
                return True, f"正在导航到区域: {target_area.name}"
            else:
                self.status_var.set(f"未知当前位置，无法导航到: {target_area.name}")
                return False, f"未知当前位置，无法导航到: {target_area.name}"
        else:
            self.status_var.set(f"未找到区域: {area_name}")
            return False, f"未找到区域: {area_name}"

    def send_arrival_notification(self):
        """发送到达通知到text3主题，通知控制AI可以执行后续任务"""
        try:
            # 创建一个临时socket连接到巴法云
            temp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            temp_socket.settimeout(10)
            temp_socket.connect((self.bf_client.BEMFA_CONFIG["host"], self.bf_client.BEMFA_CONFIG["port"]))
            
            # 发送认证信息
            auth_msg = f"{self.bf_client.BEMFA_CONFIG['uid']}\n"
            temp_socket.sendall(auth_msg.encode())
            
            # 构造到达通知消息
            if self.current_area:
                location = self.current_area.name
            elif self.current_pos:
                location = f"位置({self.current_pos[1]},{self.current_pos[0]})"
            else:
                location = "未知位置"
                
            arrival_msg = f"ARRIVAL_NOTIFICATION:小车已到达{location}"
            
            # 发送到text3主题
            pub_cmd = (
                f"cmd=2&topic=text3&"
                f"msg={arrival_msg}&"
                f"uid={self.bf_client.BEMFA_CONFIG['uid']}&"
                f"from={self.bf_client.BEMFA_CONFIG['uid']}\n"
            )
            temp_socket.sendall(pub_cmd.encode())
            print(f"已发送到达通知: {arrival_msg}")
            
            # 关闭临时连接
            temp_socket.close()
            return True
        except Exception as e:
            print(f"发送到达通知失败: {e}")
            return False

    def process_area_navigate_command(self, message):
        """处理来自控制AI的区域导航命令"""
        # 检查消息是否是区域导航命令
        if message.startswith("AREA_NAVIGATE:"):
            # 提取区域名称
            area_name = message[len("AREA_NAVIGATE:"):]
            print(f"收到区域导航命令，目标区域: {area_name}")
            
            # 导航到指定区域
            success, result = self.navigate_to_area_by_name(area_name)
            
            if success:
                self.status_var.set(f"正在导航到区域: {area_name}")
            else:
                self.status_var.set(f"导航失败: {result}")
                
                # 如果导航失败，发送TTS消息告知
                tts_message = f"抱歉，我无法导航到{area_name}，原因是{result}"
                self.bf_client.send_tts_message(tts_message)
            
            return True
        return False


if __name__ == "__main__":
    root = tk.Tk()
    app = RobotNavigationApp(root)
    
    # 创建一个单独的线程来监听到达通知
    def listen_for_arrival_notifications():
        """监听来自控制AI的到达通知"""
        # 创建一个临时socket连接到巴法云，专门用于监听text3主题
        try:
            # 创建套接字
            temp_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            temp_socket.settimeout(10)
            temp_socket.connect((app.bf_client.BEMFA_CONFIG["host"], app.bf_client.BEMFA_CONFIG["port"]))
            
            # 发送认证信息
            auth_msg = f"{app.bf_client.BEMFA_CONFIG['uid']}\n"
            temp_socket.sendall(auth_msg.encode())
            time.sleep(1)
            
            # 订阅text3主题
            subscribe_cmd = f"cmd=1&topic=text3&uid={app.bf_client.BEMFA_CONFIG['uid']}&type=3\n"
            temp_socket.sendall(subscribe_cmd.encode())
            print("已订阅text3主题，等待到达通知...")
            
            # 循环接收数据
            buffer = ""
            while True:
                try:
                    data = temp_socket.recv(1024).decode()
                    if not data:
                        print("连接已关闭，重新连接...")
                        break
                        
                    buffer += data
                    
                    # 处理可能的多行消息
                    while "\n" in buffer:
                        line, buffer = buffer.split("\n", 1)
                        if "ARRIVAL_NOTIFICATION:" in line:
                            print(f"收到到达通知: {line}")
                            # 提取位置信息
                            location = line.split("ARRIVAL_NOTIFICATION:")[1].split("&")[0]
                            # 发送TTS消息
                            app.bf_client.send_tts_message(f"我已经到达{location}，正在执行任务")
                except socket.timeout:
                    continue
                except Exception as e:
                    print(f"接收数据错误: {e}")
                    break
        except Exception as e:
            print(f"监听线程错误: {e}")
    
    # 启动监听线程
    notification_thread = threading.Thread(target=listen_for_arrival_notifications)
    notification_thread.daemon = True
    notification_thread.start()
    
    root.mainloop()