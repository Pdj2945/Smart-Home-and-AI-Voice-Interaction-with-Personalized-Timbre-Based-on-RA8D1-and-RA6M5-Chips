#include "tcp_uart.h"
#include <ctype.h>  // 添加ctype.h头文件，用于tolower函数
// 注释掉TTS功能头文件
// #include "tts_functions.h"  // 添加TTS功能头文件

// 引用hal_entry.c中的变量
extern uint32 system_time;  // 引用system_time变量

// 引用hal_entry.c中的命令常量
extern const char* CMD_TURN_LEFT;
extern const char* CMD_TURN_RIGHT;
extern const char* CMD_TURN_BACK;
extern const char* CMD_FORWARD;
extern const char* CMD_MOVE_STEPS;

// 全局变量
uint8_t tcp_connected = 0;               // 连接状态
// 注意：car_control_busy现在定义在hal_entry.c中
extern uint32_t last_position_report_time;  // 声明外部变量
static uint32_t last_heartbeat_time = 0; // 上次心跳时间
static uint32_t last_print_time = 0;     // 上次状态打印时间
static uint8_t receive_buffer[1024];     // 接收缓冲区
static char message_buffer[1024];        // AI回复消息缓冲区
static uint8_t new_message_flag = 0;     // 新消息标志

// 获取系统时间（毫秒）
uint32_t get_system_time_ms(void)
{
    return system_time;
}

// 初始化TCP连接
uint8_t tcp_uart_init(void)
{
    printf("\r\n=============== TCP通信初始化开始 ===============");
    printf("\r\n配置信息:");
    printf("\r\nWiFi名称: %s", WIFI_SSID);
    printf("\r\nWiFi密码: %s", WIFI_PASSWORD);
    printf("\r\n服务器: %s:%s", BAFA_HOST, BAFA_PORT);
    printf("\r\n密钥: %s", BAFA_KEY);
    printf("\r\n主题: %s", BAFA_TOPIC);
    
    // 延时1秒再开始连接
    system_delay_ms(1000);
    
    // 检查并初始化WiFi
    printf("\r\n开始初始化WiFi...");
    system_delay_ms(100);
    
    // 尝试初始化WiFi
    printf("\r\n尝试连接WiFi: %s", WIFI_SSID);
    int retries = 0;
    while (wifi_uart_init(WIFI_SSID, WIFI_PASSWORD, WIFI_UART_STATION))
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
    
    // 输出WiFi信息
    printf("\r\n模块版本: %s", wifi_uart_information.wifi_uart_version);
    printf("\r\nMAC地址: %s", wifi_uart_information.wifi_uart_mac);
    printf("\r\nIP地址: %s", wifi_uart_information.wifi_uart_local_ip);
    printf("\r\n端口: %s", wifi_uart_information.wifi_uart_local_port);
    
    // 延时1秒再连接服务器
    system_delay_ms(1000);
    
    // 连接TCP服务器
    printf("\r\n尝试连接TCP服务器: %s:%s", BAFA_HOST, BAFA_PORT);
    retries = 0;
    while (wifi_uart_connect_tcp_servers(BAFA_HOST, BAFA_PORT, WIFI_UART_COMMAND))
    {
        printf(".");
        retries++;
        system_delay_ms(500);
        if (retries >= 5) {
            printf("\r\n连接TCP服务器失败!");
            return 1;
        }
    }
    printf("\r\n连接TCP服务器成功!");
    
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
            printf("\r\n=============== TCP初始化完成 ===============");
            tcp_connected = 1;
            last_heartbeat_time = get_system_time_ms();
            last_position_report_time = get_system_time_ms(); // 初始化位置上报时间
            return 0;  // 成功
        }
    }
    
    printf("\r\n订阅主题失败,接收响应超时!");
    printf("\r\n=============== TCP初始化失败 ===============");
    return 1;  // 失败
}

// 向TCP服务器发送消息
uint8_t tcp_uart_send_message(const char* message)
{
    if (!tcp_connected)
    {
        printf("\r\n未连接到服务器，无法发送消息");
        return 1;
    }
    
    char send_msg[1024];
    snprintf(send_msg, sizeof(send_msg), "cmd=2&uid=%s&topic=%s&msg=%s\r\n", BAFA_KEY, BAFA_TOPIC, message);
    
    if (wifi_uart_send_buffer((uint8_t *)send_msg, strlen(send_msg)) != 0)
    {
        printf("\r\n发送消息到服务器失败");
        tcp_connected = 0;  // 标记为断开连接
        return 1;
    }
    
    printf("\r\n成功发送消息到服务器，内容: [%s], 长度: %d", message, strlen(message));
    return 0;  // 成功
}

// 注意：report_position_to_cloud函数现在定义在hal_entry.c中

// 发送心跳包
static void tcp_send_heartbeat(void)
{
    if (!tcp_connected)
        return;
    
    char heartbeat_msg[] = "ping\r\n";
    uint32_t current_time = get_system_time_ms();
    
    // 检查是否需要发送心跳包（5秒一次）
    if (current_time - last_heartbeat_time >= HEARTBEAT_INTERVAL)
    {
        if (wifi_uart_send_buffer((uint8_t *)heartbeat_msg, strlen(heartbeat_msg)) == 0)
        {
            last_heartbeat_time = current_time;
            
            // 限制打印频率（每分钟打印一次）
            if (current_time - last_print_time >= PRINT_INTERVAL) {
                printf("\r\n发送心跳包成功");
                last_print_time = current_time;
            }
        }
        else
        {
            tcp_connected = 0;
            printf("\r\n发送心跳包失败，连接已断开");
        }
    }
}

// 解析接收到的消息
static void parse_received_message(const char* message)
{
    // 检查是否是消息响应 (cmd=2&uid=xxx&topic=xxx&msg=xxx)
    char* msg_part = strstr(message, "&msg=");
    if (msg_part != NULL)
    {
        msg_part += 5; // 跳过"&msg="

        // 提取真正的命令内容，忽略后面可能的附加参数
        char command_buffer[128];
        strncpy(command_buffer, msg_part, sizeof(command_buffer) - 1);
        command_buffer[sizeof(command_buffer) - 1] = '\0';
        
        // 查找第一个&符号或结束符，截断命令
        char* end_ptr = strchr(command_buffer, '&');
        if (end_ptr != NULL) {
            *end_ptr = '\0'; // 截断字符串
        } else {
            // 如果没有&符号，查找回车换行
            end_ptr = strstr(command_buffer, "\r\n");
            if (end_ptr != NULL) {
                *end_ptr = '\0'; // 截断字符串
            }
        }
        
        // 将命令转换为小写
        for (int i = 0; command_buffer[i]; i++) {
            command_buffer[i] = tolower(command_buffer[i]);
        }
        
        printf("\r\n收到原始指令: %s", command_buffer);
        
        // 检查是否是复合指令（前置指令|控制指令）
        char* sep = strchr(command_buffer, '|');
        if (sep != NULL) {
            // 复合指令
            *sep = '\0'; // 分割字符串
            char* prep_cmd = command_buffer; // 前置指令
            char* move_cmd = sep + 1;       // 控制指令
            
            printf("\r\n解析为前置指令: %s, 控制指令: %s", prep_cmd, move_cmd);
            
            // 拷贝前置指令到消息缓冲区并立即执行
            strncpy(message_buffer, prep_cmd, sizeof(message_buffer) - 1);
            message_buffer[sizeof(message_buffer) - 1] = '\0';
            
            printf("\r\n执行前置指令: %s", message_buffer);
            // 调用外部处理函数处理前置指令
            extern void process_motor_command(const char* command);
            process_motor_command(message_buffer);
            
            // 前置指令执行完后，执行控制指令
            strncpy(message_buffer, move_cmd, sizeof(message_buffer) - 1);
            message_buffer[sizeof(message_buffer) - 1] = '\0';
            
            printf("\r\n执行控制指令: %s", message_buffer);
            new_message_flag = 1; // 标记有新消息
            process_motor_command(message_buffer);
        } else {
            // 单一指令
            strncpy(message_buffer, command_buffer, sizeof(message_buffer) - 1);
            message_buffer[sizeof(message_buffer) - 1] = '\0';
            
            printf("\r\n接收到指令: %s", message_buffer);
            new_message_flag = 1; // 标记有新消息
            
            // 处理特殊命令模式，直接在这里处理命令而不是等待主循环
            if (strcmp(message_buffer, CMD_TURN_LEFT) == 0 || 
                strcmp(message_buffer, CMD_TURN_RIGHT) == 0 ||
                strcmp(message_buffer, CMD_TURN_BACK) == 0 ||
                strcmp(message_buffer, CMD_FORWARD) == 0 ||
                strncmp(message_buffer, CMD_MOVE_STEPS, strlen(CMD_MOVE_STEPS)) == 0 ||
                strncmp(message_buffer, "mode(", 5) == 0 || 
                strcmp(message_buffer, "init()") == 0 || 
                strcmp(message_buffer, "position()") == 0 ||
                strncmp(message_buffer, "calib(", 6) == 0 ||
                strncmp(message_buffer, "exe:", 4) == 0 ||
                strcmp(message_buffer, "status") == 0)
            {
                // 调用外部处理函数（在hal_entry.c中定义）
                extern void process_motor_command(const char* command);
                process_motor_command(message_buffer);
            }
        }
    }
}

// 获取新消息
uint8_t tcp_uart_get_new_message(char* buffer, uint16_t buffer_size)
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

// TCP客户端处理函数
void tcp_uart_process(void)
{
    if (!tcp_connected)
        return;
    
    // 发送心跳包
    tcp_send_heartbeat();
    
    // 检查是否需要上报位置
    extern void report_position_to_cloud(void);  // 声明在hal_entry.c中实现的函数
    report_position_to_cloud();
    
    // 检查是否有接收到数据
    uint32_t len = wifi_uart_read_buffer(receive_buffer, sizeof(receive_buffer));
    if (len > 0)
    {
        receive_buffer[len] = '\0';
        
        // 过滤掉心跳响应消息，不打印
        if (strstr((char*)receive_buffer, "cmd=0&res=1") == NULL) {
            printf("\r\n收到TCP数据: %s", receive_buffer);
            
            // 先处理可能的TTS命令或分段消息
            // process_cloud_response((const char*)receive_buffer, len);
            
            // 然后解析其他类型的命令（电机控制等）
            parse_received_message((char*)receive_buffer);
        }
    }
} 