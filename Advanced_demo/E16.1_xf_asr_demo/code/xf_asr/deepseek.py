import paho.mqtt.client as mqtt
import json
import requests
import time
import logging

# 配置日志
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s: %(message)s')
logger = logging.getLogger(__name__)

# 配置信息
MQTT_BROKER = "bemfa.com"
MQTT_PORT = 9504
MQTT_TOPIC = "text1"
MQTT_CLIENT_ID = "your_client_id"  # 替换为你的巴法云客户端ID
MQTT_USERNAME = "your_username"    # 替换为你的巴法云用户名
MQTT_PASSWORD = "your_password"    # 替换为你的巴法云密码
DEEPSEEK_API_KEY = "your_api_key"  # 替换为你的DeepSeek API密钥
DEEPSEEK_API_URL = "https://api.deepseek.com/v1/chat/completions"

# 回调函数：连接成功
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        logger.info("成功连接到巴法云MQTT服务器")
        client.subscribe(MQTT_TOPIC)
        logger.info(f"已订阅主题: {MQTT_TOPIC}")
    else:
        logger.error(f"连接失败，返回码: {rc}")

# 回调函数：接收消息
def on_message(client, userdata, msg):
    try:
        # 解码消息
        message = msg.payload.decode('utf-8')
        logger.info(f"收到消息: {message}")
        
        # 调用DeepSeek API
        response = call_deepseek_api(message)
        
        # 将DeepSeek的响应发送回巴法云
        client.publish(MQTT_TOPIC, response)
        logger.info(f"已发送消息到巴法云: {response}")
        
    except Exception as e:
        logger.error(f"处理消息时出错: {str(e)}")

# 调用DeepSeek API
def call_deepseek_api(message):
    try:
        headers = {
            "Authorization": f"Bearer {DEEPSEEK_API_KEY}",
            "Content-Type": "application/json"
        }
        
        data = {
            "model": "deepseek-chat",
            "messages": [
                {"role": "user", "content": message}
            ],
            "temperature": 0.7
        }
        
        logger.info("正在调用DeepSeek API...")
        response = requests.post(DEEPSEEK_API_URL, headers=headers, json=data)
        response.raise_for_status()
        
        # 解析响应
        result = response.json()
        return result['choices'][0]['message']['content']
        
    except Exception as e:
        logger.error(f"调用DeepSeek API时出错: {str(e)}")
        return f"处理消息时出错: {str(e)}"

def main():
    # 创建MQTT客户端
    client = mqtt.Client(MQTT_CLIENT_ID)
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    
    # 设置回调
    client.on_connect = on_connect
    client.on_message = on_message
    
    # 连接并开始循环
    try:
        logger.info("正在连接到巴法云MQTT服务器...")
        client.connect(MQTT_BROKER, MQTT_PORT, 60)
        
        logger.info("启动云端桥接服务...")
        client.loop_forever()
        
    except KeyboardInterrupt:
        logger.info("程序被手动终止")
    except Exception as e:
        logger.error(f"发生错误: {str(e)}")
        # 失败后等待5秒再重试
        time.sleep(5)
        main()  # 重试

if __name__ == "__main__":
    main()