#include "baifa_tcp.h"
#include "zf_driver_gpio.h"
#include "asr_ctrl.h"

// 修改获取毫秒时间函数，使用静态变量实现
uint32_t pit_ms_get(void)
{
    static uint32_t ms_count = 0;
    ms_count += 5; // 每次调用增加5ms
    return ms_count;
}

// 变量定义
uint8_t baifa_connected = 0;         // 连接状态 - 改为全局变量，允许其他文件访问
static uint32_t last_heartbeat_time = 0;    // 上次心跳时间
static uint32_t last_heartbeat_print = 0;   // 上次心跳打印时间
static uint8_t receive_buffer[1024];        // 接收缓冲区
static char message_buffer[1024];           // AI回复消息缓冲区
static uint8_t new_message_flag = 0;        // 新消息标志

// 心跳包间隔时间，修改为5秒
#define HEARTBEAT_SEND_INTERVAL  5000     // 5秒发送一次心跳包
#define HEARTBEAT_PRINT_INTERVAL 180000   // 3分钟打印一次心跳日志

// 初始化巴法云TCP连接
uint8_t baifa_tcp_init(void)
{
    printf("\r\n=============== 巴法云TCP初始化开始 ===============");
    printf("\r\n配置信息:");
    printf("\r\nWiFi名称: %s", ASR_WIFI_SSID);
    printf("\r\nWiFi密码: %s", ASR_WIFI_PASSWORD);
    printf("\r\n服务器: %s:%s", BAFA_HOST, BAFA_PORT);
    printf("\r\n密钥: %s", BAFA_KEY);
    printf("\r\n主题: %s", BAFA_TOPIC);
    
    // 延时1秒再开始连接
    system_delay_ms(1000);
    
    // 检查并初始化WiFi
    printf("\r\n开始初始化WiFi...");
    system_delay_ms(100);
    
    // 使用非阻塞的方式尝试初始化WiFi
    printf("\r\n尝试连接WiFi: %s", ASR_WIFI_SSID);
    int retries = 0;
    while (wifi_uart_init(ASR_WIFI_SSID, ASR_WIFI_PASSWORD, WIFI_UART_STATION))
    {
        printf(".");
        retries++;
        system_delay_ms(500);
        if (retries >= 5) {
            printf("\r\nWiFi初始化失败,请检查WiFi名称和密码!");
            return 1;
        }
    }
    printf("\r\nWiFi初始化成功!");
    
    // 延时1秒再连接服务器
    system_delay_ms(1000);
    
    // 连接巴法云TCP服务器
    printf("\r\n尝试连接巴法云TCP服务器: %s:%s", BAFA_HOST, BAFA_PORT);
    retries = 0;
    while (wifi_uart_connect_tcp_servers(BAFA_HOST, BAFA_PORT, WIFI_UART_COMMAND))
    {
        printf(".");
        retries++;
        system_delay_ms(500);
        if (retries >= 5) {
            printf("\r\n连接巴法云TCP服务器失败!");
            return 1;
        }
    }
    printf("\r\n连接巴法云TCP服务器成功!");
    
    // 延时1秒再订阅主题
    system_delay_ms(1000);
    
    // 发送订阅命令
    char subscribe_msg[128];
    sprintf(subscribe_msg, "cmd=1&uid=%s&topic=%s\r\n", BAFA_KEY, BAFA_TOPIC);
    printf("\r\n发送订阅命令: %s", subscribe_msg);
    wifi_uart_send_buffer((uint8_t *)subscribe_msg, strlen(subscribe_msg));
    
    // 等待并检查订阅响应
    printf("\r\n等待订阅响应...");
    system_delay_ms(1000);
    uint32_t len = wifi_uart_read_buffer(receive_buffer, sizeof(receive_buffer));
    if (len > 0)
    {
        receive_buffer[len] = '\0';
        printf("\r\n收到订阅响应: %s", receive_buffer);
        
        if (strstr((char*)receive_buffer, "cmd=1&res=1"))
        {
            printf("\r\n订阅主题成功: %s", BAFA_TOPIC);
            printf("\r\n=============== 巴法云TCP初始化完成 ===============");
            baifa_connected = 1;
            last_heartbeat_time = pit_ms_get();
            return 0;  // 成功
        }
    }
    
    printf("\r\n订阅主题失败,接收响应超时!");
    printf("\r\n=============== 巴法云TCP初始化失败 ===============");
    return 1;  // 失败
}

// 向巴法云发送消息
uint8_t baifa_tcp_send_message(const char* message)
{
    if (!baifa_connected)
    {
        printf("\r\n未连接到巴法云，无法发送消息");
        return 1;
    }
    
    char send_msg[1024];
    snprintf(send_msg, sizeof(send_msg), "cmd=2&uid=%s&topic=%s&msg=%s\r\n", BAFA_KEY, BAFA_TOPIC, message);
    
    if (wifi_uart_send_buffer((uint8_t *)send_msg, strlen(send_msg)) != 0)
    {
        printf("\r\n发送消息到巴法云失败");
        baifa_connected = 0;  // 标记为断开连接
        return 1;
    }
    
    printf("\r\n成功发送消息到巴法云，内容长度: %d", strlen(message));
    return 0;  // 成功
}

// 发送心跳包
void baifa_tcp_send_heartbeat(void)
{
    if (!baifa_connected)
        return;
    
    char heartbeat_msg[] = "ping\r\n";
    uint32_t current_time = pit_ms_get();
    extern uint8 audio_server_link_flag; // 声明语音识别标志
    
    // 检查是否需要发送心跳包（5秒一次）
    if ((current_time - last_heartbeat_time >= HEARTBEAT_SEND_INTERVAL) && !audio_server_link_flag)
    {
        if (wifi_uart_send_buffer((uint8_t *)heartbeat_msg, strlen(heartbeat_msg)) == 0)
        {
            last_heartbeat_time = current_time;
            
            // 限制心跳包打印频率，每3分钟只打印一次
            if (current_time - last_heartbeat_print >= HEARTBEAT_PRINT_INTERVAL) {
                printf("\r\n发送心跳包成功");
                last_heartbeat_print = current_time;
            }
        }
        else
        {
            // 心跳失败，但不打印，防止刷屏
            baifa_connected = 0;
        }
    }
}

// 解析接收到的消息
static void parse_received_message(const char* message)
{
    // 检查是否是AI回复消息 (cmd=2&uid=xxx&topic=xxx&msg=xxx)
    char* msg_part = strstr(message, "&msg=");
    if (msg_part != NULL)
    {
        msg_part += 5; // 跳过"&msg="
        
        // 检查是否有后续内容
        char* end_ptr = strstr(msg_part, "\r\n");
        if (end_ptr != NULL)
        {
            *end_ptr = '\0';
        }
        
        // 复制消息内容
        strncpy(message_buffer, msg_part, sizeof(message_buffer) - 1);
        message_buffer[sizeof(message_buffer) - 1] = '\0';
        
        // 设置新消息标志
        new_message_flag = 1;
        
        printf("\r\n收到来自巴法云的回复: %s", message_buffer);
    }
}

// 获取新消息
uint8_t baifa_tcp_get_new_message(char* buffer, uint16_t buffer_size)
{
    if (new_message_flag)
    {
        strncpy(buffer, message_buffer, buffer_size - 1);
        buffer[buffer_size - 1] = '\0';
        new_message_flag = 0;
        return 1;  // 有新消息
    }
    return 0;  // 无新消息
}

// 巴法云TCP客户端处理函数
void baifa_tcp_process(void)
{
    if (!baifa_connected)
        return;
    
    // 发送心跳包
    baifa_tcp_send_heartbeat();
    
    // 检查是否有接收到数据
    uint32_t len = wifi_uart_read_buffer(receive_buffer, sizeof(receive_buffer));
    if (len > 0)
    {
        receive_buffer[len] = '\0';
        
        // 过滤掉心跳响应消息，不打印
        if (strstr((char*)receive_buffer, "cmd=0&res=1") == NULL) {
            printf("\r\n收到数据: %s", receive_buffer);
        }
        
        // 解析接收到的消息
        parse_received_message((char*)receive_buffer);
    }
}

// 获取是否已经连接到巴法云
uint8_t baifa_tcp_is_connected(void)
{
    return baifa_connected;
} 