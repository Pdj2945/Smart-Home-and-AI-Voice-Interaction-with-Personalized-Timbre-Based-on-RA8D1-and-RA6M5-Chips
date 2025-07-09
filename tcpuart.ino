#include <WiFi.h>
#include <DHT.h>

// WiFi配置
const char* ssid = "Redmi K50 Pro";
const char* password = "pdj2945237036";

// 巴法云TCP配置
const char* tcp_server = "bemfa.com";
const uint16_t tcp_port = 8344; // 巴法云TCP端口
const char* uid = "11ca31c562d14ef786a0238a03a3be16"; // UID
const char* topic_tts_stt = "text1";  // TTS/STT主题
const char* topic_sensor_control = "text2";  // 传感器数据和控制指令主题
const char* topic_reserved = "text3";  // 预留主题

#define MAX_MESSAGE_SIZE 256 // 最大消息长度
#define KEEP_ALIVE_INTERVAL 60 // 心跳间隔（秒）
#define STATUS_PRINT_INTERVAL 5000 // 状态打印间隔（毫秒）

// DHT11配置
#define DHTPIN 19
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// LED和蜂鸣器控制引脚
#define LED_PIN 12     // ESP32控制LED的引脚
#define BUZZER_PIN 13  // ESP32控制蜂鸣器的引脚

// UART配置 - 用于与RA6M5通信
#define RA6M5_RX_PIN 16  // ESP32 RX pin (connects to RA6M5 TX)
#define RA6M5_TX_PIN 17  // ESP32 TX pin (connects to RA6M5 RX)
#define RA6M5_UART_BAUD 115200
#define RA6M5_UART_PORT 2  // 使用Serial2

// RA6M5数据缓冲区
char ra6m5_buffer[256];
int ra6m5_buffer_index = 0;
bool ra6m5_data_ready = false;

// RA6M5传感器数据 - 重命名为实际功能
int flame_value = 0;    // 火焰传感器 (ADC0)
int smoke_value = 0;    // 烟雾传感器 (ADC4)
int light_value = 0;    // 光敏传感器 (ADC7)

// LED和蜂鸣器状态
bool led_state = false;
bool buzzer_state = false;
String led_mode = "OFF";     // OFF, ON, BLINK
String buzzer_mode = "OFF";  // OFF, ON, BEEP, SOS
bool auto_mode = false;      // 自动模式标志

// 控制相关定时器
unsigned long led_blink_timer = 0;
unsigned long buzzer_beep_timer = 0;
unsigned long auto_mode_timer = 0;
unsigned long status_print_timer = 0;

// SOS模式相关变量
int sos_state = 0;  // SOS状态机
unsigned long sos_timer = 0;

WiFiClient tcpclient;

unsigned long lastSendTime = 0; // 上次发送时间
unsigned long lastHeartbeat = 0; // 上次心跳时间

void wifi_connect() {
  Serial.print("连接WiFi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi已连接");
}

void sendtoTcpSever(String P)
{
  if(!tcpclient.connected())
  {
    Serial.println("Client is not ready");
    return;
  }
  tcpclient.print(P);
}

void tcp_connect() {
  Serial.print("连接TCP服务器");
  if(tcpclient.connect(tcp_server, tcp_port))
  {
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n",tcp_server,tcp_port);
    
    // 订阅TTS/STT主题
    String tcpTemp = "cmd=1&uid=" + String(uid) + "&topic=" + String(topic_tts_stt) + "\r\n";
    sendtoTcpSever(tcpTemp);
    Serial.println("已订阅TTS/STT主题: " + String(topic_tts_stt));
    delay(500);
    
    // 订阅传感器数据和控制指令主题
    tcpTemp = "cmd=1&uid=" + String(uid) + "&topic=" + String(topic_sensor_control) + "\r\n";
    sendtoTcpSever(tcpTemp);
    Serial.println("已订阅传感器/控制主题: " + String(topic_sensor_control));
    delay(500);
    
    // 订阅预留主题
    tcpTemp = "cmd=1&uid=" + String(uid) + "&topic=" + String(topic_reserved) + "\r\n";
    sendtoTcpSever(tcpTemp);
    Serial.println("已订阅预留主题: " + String(topic_reserved));
    
    tcpclient.setNoDelay(true);
  }
  else{
    Serial.print("NOT Connected!");
    Serial.println(tcp_server);
    tcpclient.stop();
  }
}

// 解析从RA6M5接收到的数据
void parseRA6M5Data(String data) {
  // 格式: "RA6M5:ADC0=xxxx,ADC4=xxxx,ADC7=xxxx"
  if (data.indexOf("RA6M5:") >= 0) {
    int adc0_pos = data.indexOf("ADC0=");
    int adc4_pos = data.indexOf("ADC4=");
    int adc7_pos = data.indexOf("ADC7=");
    
    if (adc0_pos >= 0 && adc4_pos >= 0 && adc7_pos >= 0) {
      // 解析ADC数值
      flame_value = data.substring(adc0_pos + 5, adc4_pos - 1).toInt();
      smoke_value = data.substring(adc4_pos + 5, adc7_pos - 1).toInt();
      
      int end_pos = data.indexOf(",", adc7_pos);
      if (end_pos > 0) {
        light_value = data.substring(adc7_pos + 5, end_pos).toInt();
      } else {
        light_value = data.substring(adc7_pos + 5).toInt();
      }
      
      // 只在状态打印时间到达时打印
      if (millis() - status_print_timer >= STATUS_PRINT_INTERVAL) {
        Serial.println("从RA6M5接收到传感器数据:");
        Serial.print("火焰传感器="); Serial.print(flame_value);
        Serial.print(", 烟雾传感器="); Serial.print(smoke_value);
        Serial.print(", 光敏传感器="); Serial.println(light_value);
        Serial.print("光照强度: "); 
        if (light_value < 1000) Serial.println("强");
        else if (light_value < 2000) Serial.println("中");
        else Serial.println("弱");
        status_print_timer = millis();
      }
    }
  }
}

// 处理从云平台接收到的命令
void processCloudCommand(String command, String topic) {
  // 忽略自己上传的数据和响应消息
  if (command.indexOf("cmd=2&res=") >= 0 || 
      command.indexOf("温度=") >= 0 || 
      command.indexOf("火焰传感器=") >= 0) {
    return;
  }
  
  Serial.print("处理云平台命令 (主题: ");
  Serial.print(topic);
  Serial.print("): ");
  Serial.println(command);
  
  // 如果是来自text2主题的控制指令
  if (topic == topic_sensor_control) {
    // 处理LED控制命令 - 支持两种格式: LED=ON 和 LED:ON
    if (command.indexOf("LED=ON") >= 0 || command.indexOf("LED:ON") >= 0) {
      led_mode = "ON";
      led_state = true;
      Serial.println("LED模式设置为ON");
    } else if (command.indexOf("LED=OFF") >= 0 || command.indexOf("LED:OFF") >= 0) {
      led_mode = "OFF";
      led_state = false;
      Serial.println("LED模式设置为OFF");
    } else if (command.indexOf("LED=BLINK") >= 0 || command.indexOf("LED:BLINK") >= 0) {
      led_mode = "BLINK";
      Serial.println("LED模式设置为BLINK");
    }
    
    // 处理蜂鸣器控制命令 - 支持两种格式
    if (command.indexOf("BUZZER=ON") >= 0 || command.indexOf("BUZZER:ON") >= 0) {
      buzzer_mode = "ON";
      buzzer_state = true;
      Serial.println("蜂鸣器模式设置为ON");
    } else if (command.indexOf("BUZZER=OFF") >= 0 || command.indexOf("BUZZER:OFF") >= 0) {
      buzzer_mode = "OFF";
      buzzer_state = false;
      Serial.println("蜂鸣器模式设置为OFF");
    } else if (command.indexOf("BUZZER=BEEP") >= 0 || command.indexOf("BUZZER:BEEP") >= 0) {
      buzzer_mode = "BEEP";
      buzzer_state = true;
      Serial.println("蜂鸣器模式设置为BEEP");
    } else if (command.indexOf("BUZZER=SOS") >= 0 || command.indexOf("BUZZER:SOS") >= 0) {
      buzzer_mode = "SOS";
      buzzer_state = true;
      sos_state = 0;  // 重置SOS状态
      Serial.println("蜂鸣器模式设置为SOS");
    }
    
    // 处理模式控制命令 - 支持两种格式
    if (command.indexOf("MODE=AUTO") >= 0 || command.indexOf("MODE:AUTO") >= 0) {
      auto_mode = true;
      auto_mode_timer = millis();  // 重置自动模式计时器
      Serial.println("设置为自动模式");
    } else if (command.indexOf("MODE=MANUAL") >= 0 || command.indexOf("MODE:MANUAL") >= 0) {
      auto_mode = false;
      Serial.println("设置为手动模式");
    }
  }
  
  // 如果是来自text1主题的消息，检查是否包含控制命令关键字
  if (topic == topic_tts_stt) {
    // 处理LED控制命令 - 支持两种格式
    if (command.indexOf("LED=ON") >= 0 || command.indexOf("LED:ON") >= 0) {
      led_mode = "ON";
      led_state = true;
      Serial.println("LED模式设置为ON (来自TTS/STT)");
    } else if (command.indexOf("LED=OFF") >= 0 || command.indexOf("LED:OFF") >= 0) {
      led_mode = "OFF";
      led_state = false;
      Serial.println("LED模式设置为OFF (来自TTS/STT)");
    } else if (command.indexOf("LED=BLINK") >= 0 || command.indexOf("LED:BLINK") >= 0) {
      led_mode = "BLINK";
      Serial.println("LED模式设置为BLINK (来自TTS/STT)");
    }
    
    // 处理蜂鸣器控制命令 - 支持两种格式
    if (command.indexOf("BUZZER=ON") >= 0 || command.indexOf("BUZZER:ON") >= 0) {
      buzzer_mode = "ON";
      buzzer_state = true;
      Serial.println("蜂鸣器模式设置为ON (来自TTS/STT)");
    } else if (command.indexOf("BUZZER=OFF") >= 0 || command.indexOf("BUZZER:OFF") >= 0) {
      buzzer_mode = "OFF";
      buzzer_state = false;
      Serial.println("蜂鸣器模式设置为OFF (来自TTS/STT)");
    } else if (command.indexOf("BUZZER=BEEP") >= 0 || command.indexOf("BUZZER:BEEP") >= 0) {
      buzzer_mode = "BEEP";
      buzzer_state = true;
      Serial.println("蜂鸣器模式设置为BEEP (来自TTS/STT)");
    } else if (command.indexOf("BUZZER=SOS") >= 0 || command.indexOf("BUZZER:SOS") >= 0) {
      buzzer_mode = "SOS";
      buzzer_state = true;
      sos_state = 0;  // 重置SOS状态
      Serial.println("蜂鸣器模式设置为SOS (来自TTS/STT)");
    }
    
    // 处理模式控制命令 - 支持两种格式
    if (command.indexOf("MODE=AUTO") >= 0 || command.indexOf("MODE:AUTO") >= 0) {
      auto_mode = true;
      auto_mode_timer = millis();  // 重置自动模式计时器
      Serial.println("设置为自动模式 (来自TTS/STT)");
    } else if (command.indexOf("MODE=MANUAL") >= 0 || command.indexOf("MODE:MANUAL") >= 0) {
      auto_mode = false;
      Serial.println("设置为手动模式 (来自TTS/STT)");
    }
  }
  
  // 立即更新LED和蜂鸣器状态
  updateLEDState();
  updateBuzzerState();
}

// 更新LED状态 - 正极驱动逻辑，HIGH表示LED点亮，LOW表示LED关闭
void updateLEDState() {
  if (led_mode == "OFF") {
    digitalWrite(LED_PIN, LOW);   // 正极驱动逻辑，LOW表示LED关闭
    led_state = false;
  } 
  else if (led_mode == "ON") {
    digitalWrite(LED_PIN, HIGH);  // 正极驱动逻辑，HIGH表示LED点亮
    led_state = true;
  }
  else if (led_mode == "BLINK") {
    // BLINK模式在loop()中处理
  }
}

// 更新蜂鸣器状态
void updateBuzzerState() {
  if (buzzer_mode == "OFF") {
    digitalWrite(BUZZER_PIN, LOW);
    buzzer_state = false;
  } 
  else if (buzzer_mode == "ON") {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzer_state = true;
  }
  // BEEP和SOS模式在loop()中处理
}

// 处理自动模式
void handleAutoMode() {
  if (!auto_mode) return;
  
  // 每10秒切换一次模式
  if (millis() - auto_mode_timer >= 10000) {
    static int auto_state = 0;
    auto_state = (auto_state + 1) % 6;
    
    switch (auto_state) {
      case 0:
        led_mode = "ON";
        buzzer_mode = "OFF";
        Serial.println("自动模式: LED=ON, BUZZER=OFF");
        break;
      case 1:
        led_mode = "OFF";
        buzzer_mode = "ON";
        Serial.println("自动模式: LED=OFF, BUZZER=ON");
        break;
      case 2:
        led_mode = "BLINK";
        buzzer_mode = "OFF";
        Serial.println("自动模式: LED=BLINK, BUZZER=OFF");
        break;
      case 3:
        led_mode = "OFF";
        buzzer_mode = "BEEP";
        Serial.println("自动模式: LED=OFF, BUZZER=BEEP");
        break;
      case 4:
        led_mode = "ON";
        buzzer_mode = "SOS";
        Serial.println("自动模式: LED=ON, BUZZER=SOS");
        break;
      case 5:
        led_mode = "BLINK";
        buzzer_mode = "BEEP";
        Serial.println("自动模式: LED=BLINK, BUZZER=BEEP");
        break;
    }
    
    auto_mode_timer = millis();
  }
}

// 处理LED闪烁模式 - 正极驱动逻辑
void handleLEDBlink() {
  if (led_mode != "BLINK") return;
  
  // 每500毫秒切换一次状态
  if (millis() - led_blink_timer >= 500) {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state ? HIGH : LOW);  // 正极驱动逻辑
    led_blink_timer = millis();
  }
}

// 处理蜂鸣器BEEP模式
void handleBuzzerBeep() {
  if (buzzer_mode != "BEEP") return;
  
  // 每1000毫秒切换一次状态
  if (millis() - buzzer_beep_timer >= 1000) {
    buzzer_state = !buzzer_state;
    digitalWrite(BUZZER_PIN, buzzer_state ? HIGH : LOW);
    buzzer_beep_timer = millis();
  }
}

// 处理蜂鸣器SOS模式 - 改进为连续蜂鸣，通过时长区分长短信号
void handleBuzzerSOS() {
  if (buzzer_mode != "SOS") return;
  
  // SOS摩尔斯电码: ... --- ...
  // 短信号: 200ms on
  // 长信号: 600ms on
  // 信号间隔: 200ms
  // 字符间隔: 600ms
  // 完整SOS后暂停: 2000ms
  
  unsigned long currentTime = millis();
  
  // SOS状态机
  // 状态0-5: S (3个短信号)
  // 状态6: 字符间隔
  // 状态7-12: O (3个长信号)
  // 状态13: 字符间隔
  // 状态14-19: S (3个短信号)
  // 状态20: 完整SOS后的长暂停
  
  switch (sos_state) {
    // S (...)
    case 0: case 2: case 4: // 短信号开始
      if (!buzzer_state) {
        buzzer_state = true;
        digitalWrite(BUZZER_PIN, HIGH);
        sos_timer = currentTime;
      } else if (currentTime - sos_timer >= 200) { // 短信号持续200ms
        buzzer_state = false;
        digitalWrite(BUZZER_PIN, LOW);
        sos_timer = currentTime;
        sos_state++;
      }
      break;
      
    case 1: case 3: case 5: // 信号间隔
      if (currentTime - sos_timer >= 200) { // 间隔200ms
        sos_state++;
        sos_timer = currentTime;
      }
      break;
      
    case 6: // 字符间隔
      if (currentTime - sos_timer >= 600) { // 字符间隔600ms
        sos_state++;
        sos_timer = currentTime;
      }
      break;
      
    // O (---)
    case 7: case 9: case 11: // 长信号开始
      if (!buzzer_state) {
        buzzer_state = true;
        digitalWrite(BUZZER_PIN, HIGH);
        sos_timer = currentTime;
      } else if (currentTime - sos_timer >= 600) { // 长信号持续600ms
        buzzer_state = false;
        digitalWrite(BUZZER_PIN, LOW);
        sos_timer = currentTime;
        sos_state++;
      }
      break;
      
    case 8: case 10: case 12: // 信号间隔
      if (currentTime - sos_timer >= 200) { // 间隔200ms
        sos_state++;
        sos_timer = currentTime;
      }
      break;
      
    case 13: // 字符间隔
      if (currentTime - sos_timer >= 600) { // 字符间隔600ms
        sos_state++;
        sos_timer = currentTime;
      }
      break;
      
    // S (...)
    case 14: case 16: case 18: // 短信号开始
      if (!buzzer_state) {
        buzzer_state = true;
        digitalWrite(BUZZER_PIN, HIGH);
        sos_timer = currentTime;
      } else if (currentTime - sos_timer >= 200) { // 短信号持续200ms
        buzzer_state = false;
        digitalWrite(BUZZER_PIN, LOW);
        sos_timer = currentTime;
        sos_state++;
      }
      break;
      
    case 15: case 17: case 19: // 信号间隔
      if (currentTime - sos_timer >= 200) { // 间隔200ms
        sos_state++;
        sos_timer = currentTime;
      }
      break;
      
    case 20: // 完整SOS后的长暂停
      if (currentTime - sos_timer >= 2000) { // 长暂停2000ms
        sos_state = 0; // 重新开始SOS循环
        sos_timer = currentTime;
      }
      break;
  }
}

void setup() {
  // 初始化主串口 (调试用)
  Serial.begin(115200);
  
  // 初始化与RA6M5通信的串口
  Serial2.begin(RA6M5_UART_BAUD, SERIAL_8N1, RA6M5_RX_PIN, RA6M5_TX_PIN);
  
  // 初始化DHT11
  dht.begin();
  
  // 初始化LED和蜂鸣器引脚
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // 初始状态LED关闭（正极驱动逻辑）
  digitalWrite(BUZZER_PIN, LOW);
  
  // 连接WiFi
  wifi_connect();
  delay(1000);
  
  // 连接TCP服务器
  tcp_connect();
  
  Serial.println("ESP32初始化完成，开始工作...");
  
  // 初始化定时器
  led_blink_timer = millis();
  buzzer_beep_timer = millis();
  auto_mode_timer = millis();
  status_print_timer = millis();
}

void loop() {
  // 检查TCP连接
  if (!tcpclient.connected()) {
    Serial.println("TCP断开，尝试重连...");
    tcp_connect();
    delay(1000);
    return;
  }

  // 读取RA6M5数据
  while (Serial2.available()) {
    char c = Serial2.read();
    
    // 存储字符到缓冲区
    if (ra6m5_buffer_index < sizeof(ra6m5_buffer) - 1) {
      ra6m5_buffer[ra6m5_buffer_index++] = c;
    }
    
    // 检测行结束
    if (c == '\n') {
      ra6m5_buffer[ra6m5_buffer_index] = '\0'; // 添加字符串结束符
      parseRA6M5Data(String(ra6m5_buffer));
      ra6m5_buffer_index = 0; // 重置缓冲区
      ra6m5_data_ready = true;
    }
  }

  // 处理自动模式
  handleAutoMode();
  
  // 处理LED闪烁
  handleLEDBlink();
  
  // 处理蜂鸣器模式
  if (buzzer_mode == "BEEP") {
    handleBuzzerBeep();
  } else if (buzzer_mode == "SOS") {
    handleBuzzerSOS();
  }

  // 定时上传温湿度和RA6M5数据
  if (millis() - lastSendTime > 5000) { // 每5秒上传一次
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    
    // 构建消息 - 发送到传感器/控制主题
    String msg = "cmd=2&uid=" + String(uid) + "&topic=" + String(topic_sensor_control) + "&msg=";
    
    // 添加温湿度数据
    if (isnan(h) || isnan(t)) {
      msg += "温湿度读取失败";
    } else {
      msg += "温度=" + String(t) + "C,湿度=" + String(h) + "%";
    }
    
    // 添加传感器数据 - 使用实际功能名称
    msg += ",火焰传感器=" + String(flame_value) + 
           ",烟雾传感器=" + String(smoke_value) + 
           ",光敏传感器=" + String(light_value) +
           ",LED=" + (led_state ? "ON" : "OFF") +
           ",BUZZER=" + (buzzer_state ? "ON" : "OFF") +
           ",LED_MODE=" + led_mode +
           ",BUZZER_MODE=" + buzzer_mode +
           ",AUTO=" + (auto_mode ? "ON" : "OFF") + "\r\n";
    
    // 发送到云平台
    sendtoTcpSever(msg);
    
    // 每5秒打印一次状态
    Serial.println("状态更新:");
    Serial.print("温度: "); Serial.print(t); Serial.print("°C, 湿度: "); Serial.print(h); Serial.println("%");
    Serial.print("火焰传感器: "); Serial.print(flame_value);
    Serial.print(", 烟雾传感器: "); Serial.print(smoke_value);
    Serial.print(", 光敏传感器: "); Serial.println(light_value);
    Serial.print("LED状态: "); Serial.print(led_state ? "ON" : "OFF");
    Serial.print(", 模式: "); Serial.println(led_mode);
    Serial.print("蜂鸣器状态: "); Serial.print(buzzer_state ? "ON" : "OFF");
    Serial.print(", 模式: "); Serial.println(buzzer_mode);
    Serial.print("工作模式: "); Serial.println(auto_mode ? "AUTO" : "MANUAL");
    Serial.println("----------------------------");
    
    lastSendTime = millis();
  }

  // 心跳包，防止掉线
  if (millis() - lastHeartbeat > KEEP_ALIVE_INTERVAL * 1000) {
    String heartbeat = "cmd=0&uid=" + String(uid) + "\r\n";
    sendtoTcpSever(heartbeat);
    Serial.println("发送心跳包");
    lastHeartbeat = millis();
  }

  // 读取服务器下发消息
  while (tcpclient.available()) {
    String line = tcpclient.readStringUntil('\n');
    
    // 提取主题
    String topic = "";
    if (line.indexOf("&topic=") >= 0) {
      int topicStart = line.indexOf("&topic=") + 7;
      int topicEnd = line.indexOf("&", topicStart);
      if (topicEnd > topicStart) {
        topic = line.substring(topicStart, topicEnd);
      } else {
        topic = line.substring(topicStart);
      }
    }
    
    // 处理云平台命令
    processCloudCommand(line, topic);
  }
}