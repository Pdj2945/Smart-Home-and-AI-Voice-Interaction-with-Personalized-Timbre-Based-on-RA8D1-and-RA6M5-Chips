#include "asr_audio.h"
#include "websocket_client.h"

#define     ASR_SEND_DATA_MAX_LENTH     (4000)                          // 语音识别发送的最长语音数据
#define     SILENCE_THRESHOLD           (200)                           // 静音阈值
#define     SILENCE_TIMEOUT_MS          (3000)                          // 静音超时时间（毫秒）

// TTS相关定义
#define     TTS_CMD_PREFIX              "tts:"                          // TTS命令前缀
#define     TTS_CMD_PREFIX_LEN          4                               // TTS命令前缀长度
#define     STT_CMD_PREFIX              "stt:"                          // STT命令前缀
#define     STT_CMD_PREFIX_LEN          4                               // STT命令前缀长度

// 系统状态定义
typedef enum {
    STATE_IDLE,                 // 空闲状态，等待按键开始录音
    STATE_RECORDING             // 正在录音状态
} system_state_t;

// 声明外部函数
extern void send_utf8_text_to_tts(const uint8 *utf8_text, uint16 utf8_len);
extern void process_tts_message(const uint8 *text, uint16 len);
extern bool starts_with(const char* str, const char* prefix, int prefix_len);

char        time_now_data[32];                                          // 当前时间戳
char        asr_url_out[512];                                           // websocket_url
uint8       wifi_uart_get_data_buffer[1024];                            // wifi接收缓冲区
char        http_request[1024];                                         // http协议缓冲区

int16       adc_get_one_data;                                           // 获取麦克风一次的ADC值
int         audio_get_count = -1;                                       // 音频数据计数

fifo_struct adc_data_fifo;                                              // 麦克风数据fifo
uint32      fifo_data_count;                                            // fifo一次接收数据变量
int16       adc_get_data[ASR_SEND_DATA_MAX_LENTH * 2];                  // fifo接收数据缓冲区
int16       fifo_get_data[ASR_SEND_DATA_MAX_LENTH];                     // fifo获取缓冲区

char        json_data[ASR_SEND_DATA_MAX_LENTH * 3];                     // json发送缓冲区
char        temp_data[ASR_SEND_DATA_MAX_LENTH * 3 + 2000];              // 临时数据缓冲区
uint8       websocket_receive_buffer[4096];                             // 接收websocket数据
uint8       asr_textout[1024] = {0};                                    // 语音结果
uint8       asr_result_buffer[2048] = {0};                              // 完整的识别结果缓冲区

uint8       audio_start_flag = 0;                                       // 语音识别开始标志位
uint8       audio_need_net_flag = 0;                                    // 语音识别需要联网标志位
uint8       audio_server_link_flag = 0;                                 // 语音识别连接服务器标志位
uint8       audio_send_data_flag = 0;                                   // 语音识别发送数据标志位
uint32      asr_max_time = 0;

uint16      silence_counter = 0;                                        // 静音计数器
uint8       is_speaking = 0;                                            // 正在说话标志
uint8       new_result_available = 0;                                   // 新识别结果可用标志

// 巴法云相关变量
uint8       bafa_connected = 0;                                         // 巴法云连接状态
uint8       bafa_reconnect_count = 0;                                   // 巴法云重连计数
uint32      last_heartbeat_time = 0;                                    // 上次心跳发送时间
uint32      system_time_ms = 0;                                         // 系统时间计数器(毫秒)
uint8       bafa_cloud_reply[1024] = {0};                               // 巴法云回复内容
uint8       bafa_reply_available = 0;                                   // 巴法云回复可用标志
uint8       wifi_status = 0;                                            // WiFi连接状态：0-未初始化，1-已初始化

system_state_t current_state = STATE_IDLE;                              // 当前系统状态
uint8       button_pressed = 0;                                         // 按键按下标志
uint8       auto_end_flag = 0;                                          // 自动结束标志（检测到静音）

uint16      label_text_id;											    // 创建一个文本标签ID
uint16      label_out_id;											    // 创建一个文本标签ID
uint16      label_reply_id;                                             // 创建一个用于显示云平台回复的文本标签

// 前置声明函数
void handle_button_press(void);
void process_idle_state(void);
void process_recording_state(void);
void handle_end_of_recognition(void);

// 获取系统时间(毫秒)
uint32 get_system_time_ms(void)
{
    system_time_ms += 10; // 假设每次调用间隔约10ms
    return system_time_ms;
}

// 检查并恢复WiFi连接
uint8 check_and_restore_wifi(void)
{
    // 获取WiFi状态
    if (strstr((char*)wifi_uart_information.wifi_uart_local_ip, "0.0.0.0") || 
        strlen((char*)wifi_uart_information.wifi_uart_local_ip) == 0)
    {
        printf("\r\nWiFi连接已断开，正在重新连接...");
        // 断开已有连接
        wifi_uart_disconnect_link();
        system_delay_ms(500);
        
        // 关闭巴法云连接标志
        bafa_connected = 0;
        
        // 重新初始化WiFi
        while(wifi_uart_init(ASR_WIFI_SSID, ASR_WIFI_PASSWORD, WIFI_UART_STATION))
        {
            printf("\r\nWiFi重连失败，继续尝试...");
            system_delay_ms(1000);
        }
        
        printf("\r\nWiFi重连成功: IP=%s", wifi_uart_information.wifi_uart_local_ip);
        wifi_status = 1;
        return 1; // WiFi状态已恢复
    }
    
    return 0; // WiFi状态正常
}

// 暂时断开巴法云连接以释放资源
void disconnect_bafa_temporarily(void)
{
    if (bafa_connected)
    {
        printf("\r\n暂时断开巴法云连接以执行语音识别...");
        wifi_uart_disconnect_link();
        bafa_connected = 0;
        system_delay_ms(500);
    }
}

// 巴法云连接函数
uint8 connect_bafa_cloud(void)
{
    static uint8 retry_count = 0;
    
    // 检查WiFi状态
    if (check_and_restore_wifi())
    {
        retry_count = 0;
    }
    
    // 如果已经重试了多次，则等待一段时间再尝试
    if (retry_count >= RECONNECT_MAX_ATTEMPTS) {
        system_delay_ms(10000); // 等待10秒
        retry_count = 0;
    }
    
    // 断开任何可能的现有连接
    wifi_uart_disconnect_link();
    system_delay_ms(500);
    
    // 连接巴法云TCP服务器
    printf("\r\n连接巴法云服务器...");
    uint8 connect_attempt = 0;
    while(connect_attempt < 3) // 尝试连接最多3次
    {
        if (!wifi_uart_connect_tcp_servers(BAFA_SERVER, BAFA_PORT, WIFI_UART_COMMAND))
        {
            // 连接成功，跳出循环
            break;
        }
        
        printf("\r\n连接巴法云服务器失败，重试中...");
        connect_attempt++;
        system_delay_ms(1000); // 等待1秒后重试
    }
    
    if (connect_attempt >= 3)
    {
        retry_count++;
        if (retry_count >= RECONNECT_MAX_ATTEMPTS)
        {
            printf("\r\n多次连接失败，将暂停一段时间后再尝试");
        }
        return 1; // 连接失败
    }
    
    // 清空缓冲区，避免之前的数据干扰
    memset(wifi_uart_get_data_buffer, 0, sizeof(wifi_uart_get_data_buffer));
    wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
    
    // 订阅主题消息
    char subscribe_msg[128];
    sprintf(subscribe_msg, "cmd=1&uid=%s&topic=%s", BAFA_KEY, BAFA_TOPIC);
    
    // 尝试发送订阅请求多次
    uint8 subscribe_attempt = 0;
    uint8 subscribe_success = 0;
    
    while(subscribe_attempt < 5 && !subscribe_success) // 尝试订阅最多5次
    {
        // 发送订阅请求
        wifi_uart_send_buffer((uint8_t *)subscribe_msg, strlen(subscribe_msg));
        
        // 等待订阅响应
        system_delay_ms(1000);
        uint16 data_length = wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
        
        // 检查响应内容
        if (data_length && strstr((char*)wifi_uart_get_data_buffer, "cmd=1&res=1"))
        {
            printf("\r\n主题订阅成功");
            subscribe_success = 1;
        }
        else
        {
            // 如果收到其他响应，打印出来以便调试
            if (data_length) {
                printf("\r\n收到响应但不是订阅成功: %s", wifi_uart_get_data_buffer);
            } else {
                printf("\r\n主题订阅超时，重试中...");
            }
            subscribe_attempt++;
            system_delay_ms(500);
        }
    }
    
    if (!subscribe_success)
    {
        printf("\r\n主题订阅失败，连接过程中断");
        wifi_uart_disconnect_link();
        system_delay_ms(500);
        retry_count++;
        return 1; // 订阅失败
    }
    
    retry_count = 0;
    bafa_reconnect_count = 0;
    bafa_connected = 1;
    printf("\r\n成功连接到巴法云服务器");
    return 0; // 连接成功
}

// 发送心跳包
void send_heartbeat(void)
{
    if (bafa_connected)
    {
        // 清空缓冲区，避免之前的数据干扰
        memset(wifi_uart_get_data_buffer, 0, sizeof(wifi_uart_get_data_buffer));
        wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
        
        // 构建心跳包
        char heartbeat_msg[128];
        sprintf(heartbeat_msg, "cmd=0&uid=%s", BAFA_KEY);
        
        // 发送心跳包
        wifi_uart_send_buffer((uint8_t *)heartbeat_msg, strlen(heartbeat_msg));
        
        // 调试信息，可以注释掉以减少输出
        // printf("\r\n发送心跳包到巴法云");
    }
}

// 发送消息到巴法云
void send_message_to_bafa(char* message)
{
    if (!bafa_connected || message == NULL || strlen(message) == 0)
    {
        return;
    }
    
    // 检查是否需要重新连接
    if (!bafa_connected)
    {
        if (connect_bafa_cloud() != 0)
        {
            printf("\r\n发送失败，未连接到巴法云");
            return;
        }
    }
    
    // URL编码消息内容，避免特殊字符导致解析错误
    char encoded_message[2048];
    char* src = message;
    char* dst = encoded_message;
    
    while (*src && (dst - encoded_message < sizeof(encoded_message) - 4))
    {
        if ((*src >= 'A' && *src <= 'Z') || 
            (*src >= 'a' && *src <= 'z') || 
            (*src >= '0' && *src <= '9') ||
            *src == '-' || *src == '_' || *src == '.' || *src == '~')
        {
            // 安全字符，直接复制
            *dst++ = *src;
        }
        else if (*src == ' ')
        {
            // 空格转换为加号
            *dst++ = '+';
        }
        else
        {
            // 其他字符进行百分号编码
            sprintf(dst, "%%%02X", (unsigned char)*src);
            dst += 3;
        }
        src++;
    }
    *dst = '\0';
    
    // 发送消息
    char send_msg[2048 + 256];
    sprintf(send_msg, "cmd=2&uid=%s&topic=%s&msg=%s", BAFA_KEY, BAFA_TOPIC, encoded_message);
    
    // 清空缓冲区，避免之前的数据干扰
    memset(wifi_uart_get_data_buffer, 0, sizeof(wifi_uart_get_data_buffer));
    wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
    
    // 尝试发送多次，确保消息发送成功
    uint8 retry_count = 0;
    uint8 send_success = 0;
    
    while (retry_count < 3 && !send_success)
    {
        wifi_uart_send_buffer((uint8_t *)send_msg, strlen(send_msg));
        printf("\r\n发送数据到巴法云: [%s]", message);
        
        // 等待响应
        system_delay_ms(500);
        uint16 data_length = wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
        
        if (data_length && strstr((char*)wifi_uart_get_data_buffer, "cmd=2&res=1"))
        {
            printf("\r\n巴法云确认消息已接收");
            send_success = 1;
        }
        else
        {
            printf("\r\n未收到巴法云确认，重试中...");
            retry_count++;
            system_delay_ms(500);
        }
    }
    
    if (!send_success)
    {
        printf("\r\n发送到巴法云失败，请检查网络连接");
    }
    
    // 清空缓冲区
    memset(wifi_uart_get_data_buffer, 0, sizeof(wifi_uart_get_data_buffer));
}

// 检查并接收巴法云回复 - 修改此函数以支持TTS
void check_bafa_reply(void)
{
    if (bafa_connected && wifi_uart_get_data_buffer[0] != 0)
    {
        // 首先，直接检查是否包含TTS命令，无论消息格式如何
        char* direct_tts = strstr((char*)wifi_uart_get_data_buffer, "tts:");
        if (direct_tts)
        {
            printf("\r\n直接检测到TTS命令");
            
            // 提取纯文本内容
            char tts_text[256] = {0};
            char* text_start = direct_tts + 4; // 跳过"tts:"前缀
            
            // 查找可能的结束标记
            char* end_mark = NULL;
            end_mark = strstr(text_start, "&uid=");
            if (!end_mark) end_mark = strstr(text_start, "&from=");
            if (!end_mark) end_mark = strstr(text_start, "\r\n");
            if (!end_mark) end_mark = strstr(text_start, "cmd=");
            
            // 复制纯文本内容
            if (end_mark) {
                int text_len = end_mark - text_start;
                if (text_len > 0 && text_len < sizeof(tts_text)) {
                    strncpy(tts_text, text_start, text_len);
                    tts_text[text_len] = '\0';
                }
            } else {
                strncpy(tts_text, text_start, sizeof(tts_text) - 1);
                tts_text[sizeof(tts_text) - 1] = '\0'; // 确保字符串结束
            }
            
            printf("\r\n提取的纯文本内容: [%s]", tts_text);
            
            // 构建完整的TTS命令用于处理，但显示时只显示纯文本
            memset(bafa_cloud_reply, 0, sizeof(bafa_cloud_reply));
            strcpy((char*)bafa_cloud_reply, "tts:");
            strcat((char*)bafa_cloud_reply, tts_text);
            
            printf("\r\n构建的TTS命令: [%s]", bafa_cloud_reply);
            
            // 清除上一条TTS消息并显示新消息
            // 注意：只清除TTS回复标签，不清除语音识别结果标签
            ips200pro_label_show_string(label_reply_id, tts_text);
            
            // 执行TTS命令
            process_tts_message(bafa_cloud_reply, strlen((const char*)bafa_cloud_reply));
            
            // 清空缓冲区，避免重复处理
            memset(wifi_uart_get_data_buffer, 0, sizeof(wifi_uart_get_data_buffer));
            return;
        }
        
        // 如果没有直接找到TTS命令，尝试标准解析
        printf("\r\n处理巴法云回复");
        
        // 解析回复内容
        char* ipd_start = strstr((char*)wifi_uart_get_data_buffer, "+IPD,");
        if (ipd_start)
        {
            // 跳过"+IPD,xx:"格式
            char* content_start = strchr(ipd_start, ':');
            if (content_start)
            {
                content_start++; // 跳过冒号
                
                // 检查是否是正确响应
                if (strstr(content_start, "cmd=0&res=1") || strstr(content_start, "cmd=1&res=1"))
                {
                    // 心跳包或订阅响应，不需处理
                }
                else if (strstr(content_start, "cmd=2&topic=") && strstr(content_start, "&msg="))
                {
                    // 找到实际消息内容
                    char* msg_start = strstr(content_start, "&msg=");
                    if (msg_start)
                    {
                        msg_start += 5; // 跳过"&msg="
                        
                        // 尝试解析分段消息格式 tts(m|n):
                        if (strstr(msg_start, "tts(") && strstr(msg_start, "|") && strstr(msg_start, "):"))
                        {
                            // 查找可能的结束标记
                            char* end_mark = NULL;
                            end_mark = strstr(msg_start, "&uid=");
                            if (!end_mark) end_mark = strstr(msg_start, "&from=");
                            if (!end_mark) end_mark = strstr(msg_start, "\r\n");
                            if (!end_mark) end_mark = strstr(msg_start, "cmd=");
                            
                            // 提取消息内容
                            char msg_content[512] = {0};
                            int msg_len = 0;
                            
                            if (end_mark) {
                                msg_len = end_mark - msg_start;
                                if (msg_len > 0 && msg_len < sizeof(msg_content)) {
                                    strncpy(msg_content, msg_start, msg_len);
                                    msg_content[msg_len] = '\0';
                                }
                            } else {
                                strncpy(msg_content, msg_start, sizeof(msg_content) - 1);
                                msg_content[sizeof(msg_content) - 1] = '\0';
                                msg_len = strlen(msg_content);
                            }
                            
                            // 调用分段消息处理函数
                            printf("\r\n检测到分段消息格式，调用process_cloud_response处理");
                            process_cloud_response(msg_content, msg_len);
                        }
                        // 检查是否是TTS命令
                        else if (starts_with(msg_start, TTS_CMD_PREFIX, TTS_CMD_PREFIX_LEN)) {
                            // 提取纯文本内容
                            char tts_text[256] = {0};
                            char* text_start = msg_start + 4; // 跳过"tts:"前缀
                            
                            // 查找可能的结束标记
                            char* end_mark = NULL;
                            end_mark = strstr(text_start, "&uid=");
                            if (!end_mark) end_mark = strstr(text_start, "&from=");
                            if (!end_mark) end_mark = strstr(text_start, "\r\n");
                            if (!end_mark) end_mark = strstr(text_start, "cmd=");
                            
                            // 复制纯文本内容
                            if (end_mark) {
                                int text_len = end_mark - text_start;
                                if (text_len > 0 && text_len < sizeof(tts_text)) {
                                    strncpy(tts_text, text_start, text_len);
                                    tts_text[text_len] = '\0';
                                }
                            } else {
                                strncpy(tts_text, text_start, sizeof(tts_text) - 1);
                                tts_text[sizeof(tts_text) - 1] = '\0'; // 确保字符串结束
                            }
                            
                            printf("\r\n提取的纯文本内容: [%s]", tts_text);
                            
                            // 构建完整的TTS命令用于处理
                            memset(bafa_cloud_reply, 0, sizeof(bafa_cloud_reply));
                            strcpy((char*)bafa_cloud_reply, "tts:");
                            strcat((char*)bafa_cloud_reply, tts_text);
                            
                            printf("\r\n构建的TTS命令: [%s]", bafa_cloud_reply);
                            
                            // 清除上一条TTS消息并显示新消息
                            // 注意：只清除TTS回复标签，不清除语音识别结果标签
                            ips200pro_label_show_string(label_reply_id, tts_text);
                            
                            // 执行TTS命令
                            process_tts_message(bafa_cloud_reply, strlen((const char*)bafa_cloud_reply));
                        }
                        else {
                            // 处理可能的结束标记
                            char* end_mark = NULL;
                            
                            // 查找可能的结束标记
                            end_mark = strstr(msg_start, "&uid=");
                            if (!end_mark) end_mark = strstr(msg_start, "&from=");
                            if (!end_mark) end_mark = strstr(msg_start, "\r\n");
                            if (!end_mark) end_mark = strstr(msg_start, "cmd=");
                            
                            // 清空旧回复
                            memset(bafa_cloud_reply, 0, sizeof(bafa_cloud_reply));
                            
                            // 复制消息内容
                            if (end_mark) {
                                // 有结束标记，复制到结束标记之前的内容
                                int msg_len = end_mark - msg_start;
                                if (msg_len > 0 && msg_len < sizeof(bafa_cloud_reply)) {
                                    strncpy((char*)bafa_cloud_reply, msg_start, msg_len);
                                    bafa_cloud_reply[msg_len] = '\0';
                                }
                            } else {
                                // 没有结束标记，复制整个消息
                                strncpy((char*)bafa_cloud_reply, msg_start, sizeof(bafa_cloud_reply) - 1);
                                bafa_cloud_reply[sizeof(bafa_cloud_reply) - 1] = '\0'; // 确保字符串结束
                            }
                            
                            // 显示回复内容 - 只有TTS命令才显示到屏幕上
                            printf("\r\n解析出的回复内容: [%s]", bafa_cloud_reply);
                            
                            // 不再显示非TTS消息到屏幕上
                            // ips200pro_label_show_string(label_reply_id, (char*)bafa_cloud_reply);
                            
                            bafa_reply_available = 1;
                            
                            // 检查是否是分段消息格式
                            if (strstr((char*)bafa_cloud_reply, "tts(") || strstr((char*)bafa_cloud_reply, "stt(")) {
                                // 尝试调用分段消息处理函数
                                process_cloud_response((const char*)bafa_cloud_reply, strlen((const char*)bafa_cloud_reply));
                            }
                        }
                    }
                }
            }
        }
        
        // 清空缓冲区，避免重复处理
        memset(wifi_uart_get_data_buffer, 0, sizeof(wifi_uart_get_data_buffer));
    }
}

// 解析时间戳
void time_parse_data(char* input, char* time_str)
{
    const char* dateField = strstr(input, "Date: ");
    if(dateField == NULL) return;
    dateField += 6;
    const char* end = strchr(dateField, '\n');
    if(end == NULL) return;
    int length = end - dateField;
    strncpy(time_str, dateField, length);
    time_str[length - 1] = '\0';
}

// 获取时间戳
void time_get_data()
{
    // 确保WiFi已连接
    if (!wifi_status)
    {
        while(wifi_uart_init(ASR_WIFI_SSID, ASR_WIFI_PASSWORD, WIFI_UART_STATION))
        {
            system_delay_ms(500);
        }
        wifi_status = 1;
    }
    
    // 如果巴法云已连接，需要暂时断开
    uint8 was_connected = bafa_connected;
    if (bafa_connected)
    {
        wifi_uart_disconnect_link();
        bafa_connected = 0;
        system_delay_ms(500);
    }
    
    while(wifi_uart_connect_tcp_servers(
                ASR_TARGET_IP,
                ASR_TARGET_PORT,
                WIFI_UART_COMMAND))
    {
        printf("\r\n连接时间服务器失败，重试中...");
        system_delay_ms(500);
    }
    memset(http_request, 0, sizeof(http_request));
    snprintf(http_request, sizeof(http_request), "1");

    while(wifi_uart_send_buffer((const uint8*)http_request, strlen(http_request)));
    memset(wifi_uart_get_data_buffer, 0, sizeof(wifi_uart_get_data_buffer));
    system_delay_ms(1000);
    wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
    time_parse_data((char*)wifi_uart_get_data_buffer, time_now_data);
    
    // 断开连接
    wifi_uart_disconnect_link();
    system_delay_ms(500);
    
    // 如果之前巴法云已连接，现在需要尝试重新连接
    if (was_connected)
    {
        connect_bafa_cloud();
    }
}

// 解析语音识别字段
void extract_content_fields(const char* input)
{
    const char* ptr = input;
    char w_value[256];
    char full_result[2048] = {0};
    int has_content = 0;
    
    // 检查是否有新内容
    if(strstr(ptr, "\"w\":\"") == NULL) {
        return;  // 没有识别结果
    }
    
    // 清空临时结果缓冲区
    memset(asr_textout, 0, sizeof(asr_textout));
    
    while(*ptr)
    {
        // 查找 "w":" 模式
        const char* w_start = strstr(ptr, "\"w\":\"");
        if(!w_start)
        {
            break;
        }
        w_start += 5; // 跳过 "\"w\":\""
        const char* w_end = strchr(w_start, '"');
        if(!w_end)
        {
            break;
        }
        // 提取 w 字段的值
        int length = w_end - w_start;
        strncpy(w_value, w_start, length);
        w_value[length] = '\0';
        
        // 将词添加到临时结果
        strcat((char*)asr_textout, w_value);
        has_content = 1;
        
        ptr = w_end + 1;
    }
    
    if(has_content) {
        // 将当前识别的结果添加到完整的结果缓冲区
        strcat((char*)asr_result_buffer, (char*)asr_textout);
        
        // 打印当前识别的词
        printf("%s", asr_textout);
        
        // 在界面上显示完整的识别结果
        ips200pro_label_show_string(label_out_id, (char*)asr_result_buffer);
        
        // 设置新结果标志
        new_result_available = 1;
    }
}

// 计算websocket url
void websocket_get_url(const char* base_url, char* url_out)
{
    uint8_t APISecret[] = {ASR_APISecret};
    uint8_t APIKey[] = {ASR_APIKey};
    uint8_t indata[128] = {0};
    uint8_t sha256_out[128];
    char base64_out[512];

    char host_domain[256] = {0};
    char path[256] = {0};

    const char* domain_start = strstr(base_url, "://");
    if(domain_start)
    {
        domain_start += 3;
        const char* path_start = strchr(domain_start, '/');
        if(path_start)
        {
            strncpy(host_domain, domain_start, path_start - domain_start);
            host_domain[path_start - domain_start] = '\0';
            strcpy(path, path_start);
        }
    }

    snprintf((char*)indata, sizeof(indata),
             "host: %s\n"
             "date: %s\n"
             "GET %s HTTP/1.1", host_domain, time_now_data, path);
    
    hmac_sha256(APISecret, strlen((char*)APISecret),
                indata, strlen((char*)indata),
                sha256_out);

    base64_encode((uint8_t*)sha256_out, base64_out, SHA256_DIGESTLEN);

    char authorization_origin[256] = {0};
    snprintf(authorization_origin, sizeof(authorization_origin),
             "api_key=\"%s\", algorithm=\"hmac-sha256\", headers=\"host date request-line\", signature=\"%s\"",
             APIKey, base64_out);
    base64_encode((uint8_t*)authorization_origin, base64_out, strlen(authorization_origin));
    char encoded_authorization[256];
    url_encode(base64_out, encoded_authorization);
    char encoded_date[256];
    url_encode(time_now_data, encoded_date);
    char encoded_host[256];
    url_encode(host_domain, encoded_host);
    uint16 total_len = strlen(base_url) + strlen(encoded_authorization) + strlen(encoded_date) + strlen(encoded_host) + 100;
    snprintf(url_out, total_len, "%s?authorization=%s&date=%s&host=%s",
             base_url, encoded_authorization, encoded_date, encoded_host);
//    printf("WebSocket URL: %s \t %d\n", url_out, strlen(url_out));
}

// 语音识别定时器回调函数
void audio_callback()
{
    // 处理录音相关状态
    if(audio_start_flag)
    {
        adc_get_one_data = ((int16)adc_get_mic() - 1870);
        
        // 检测静音
        if(audio_server_link_flag)
        {
            // 如果声音大于阈值，认为是在说话
            if(abs(adc_get_one_data) > SILENCE_THRESHOLD) {
                is_speaking = 1;
                silence_counter = 0;
            }
            else if(is_speaking) {
                // 如果之前在说话，现在音量低于阈值，增加静音计数
                silence_counter++;
                
                // 如果静音时间超过阈值（8000Hz采样率下，3秒 = 24000个样本）
                if(silence_counter > (SILENCE_TIMEOUT_MS * 8)) {
                    // 如果已经在说话并且有新结果，触发自动结束
                    if(new_result_available && current_state == STATE_RECORDING) {
                        auto_end_flag = 1;
                        silence_counter = 0;
                    }
                }
            }
            
            audio_get_count++;
            fifo_write_buffer(&adc_data_fifo, &adc_get_one_data, 1);
        }
        
        if((audio_get_count + 1) % (ASR_SEND_DATA_MAX_LENTH - 500) == 0)
        {
            if(audio_server_link_flag)
            {
                asr_max_time += (ASR_SEND_DATA_MAX_LENTH - 500);
                audio_send_data_flag = 1;
            }
        }
    }
    
    // 如果达到最大录音时长，自动停止
    if(asr_max_time > (60 * 8000) && audio_server_link_flag && current_state == STATE_RECORDING)
    {
        printf("\r\n达到最大录音时长，自动停止");
        ips200pro_label_show_string(label_text_id, "达到最大录音时长，自动停止");
        
        audio_start_flag = 0;
        audio_get_count = 0;
        audio_send_data_flag = 0;
        audio_need_net_flag = 1;
    }
}

// 语音数据发送
void audio_data_send(uint8 status)
{
    if(status != 0)
    {
        fifo_data_count = fifo_used(&adc_data_fifo);
        if(fifo_data_count > ASR_SEND_DATA_MAX_LENTH)
        {
            fifo_data_count = ASR_SEND_DATA_MAX_LENTH;
        }
        memset(fifo_get_data, 0, sizeof(fifo_get_data));
        fifo_read_buffer(&adc_data_fifo, fifo_get_data, &fifo_data_count, FIFO_READ_AND_CLEAN);
        memset(temp_data, 0, sizeof(temp_data));
        base64_encode((uint8*)fifo_get_data, temp_data, fifo_data_count * 2);
        memset(json_data, 0, sizeof(json_data));
    }
    else
    {
        memset(temp_data, 0, sizeof(temp_data));
    }
    snprintf(json_data, (ASR_SEND_DATA_MAX_LENTH * 3),
             "{  \"common\":                                                                                                            \
                        {\"app_id\": \"%s\"},                                                                                           \
                    \"business\":                                                                                                       \
                        {\"domain\": \"iat\", \"language\": \"zh_cn\", \"accent\": \"mandarin\", \"vinfo\": 1, \"vad_eos\": 10000},     \
                    \"data\":                                                                                                           \
                        {\"status\": %d, \"format\": \"audio/L16;rate=8000\", \"audio\": \"%s\", \"encoding\": \"raw\"}}",              \
             ASR_APIID, status, temp_data);
}

// 语音识别初始化
void audio_init()
{
    printf("等待初始化...\r\n");
    
    // 初始化状态变量
    current_state = STATE_IDLE;
    button_pressed = 0;
    auto_end_flag = 0;
    
    ips200pro_init("语音识别测试", IPS200PRO_TITLE_BOTTOM, 30);
    ips200pro_set_direction(IPS200PRO_CROSSWISE_180);
    ips200pro_set_format(IPS200PRO_FORMAT_UTF8);
    ips200pro_set_default_font(FONT_SIZE_16);
    
    // 创建文本标签
    label_text_id = ips200pro_label_create(1, 1, 318, 30);  			// 创建状态文本标签
    ips200pro_set_color(label_text_id, COLOR_BORDER, IPS200PRO_RGB888_TO_RGB565(0,0,0));
    ips200pro_label_mode(label_text_id, LABEL_SCROLL_CIRCULAR);
    ips200pro_label_printf(label_text_id, "语音初始化...");
    
    label_out_id = ips200pro_label_create(1, 33, 318, 86);  			// 创建语音识别结果文本标签
    ips200pro_set_color(label_out_id, COLOR_BORDER, IPS200PRO_RGB888_TO_RGB565(0,0,0));
    ips200pro_label_mode(label_out_id, LABEL_AUTO);
    ips200pro_label_printf(label_out_id,"语音识别结果:");
    
    label_reply_id = ips200pro_label_create(1, 121, 318, 87);          // 创建云平台回复文本标签
    ips200pro_set_color(label_reply_id, COLOR_BORDER, IPS200PRO_RGB888_TO_RGB565(0,0,255));
    ips200pro_label_mode(label_reply_id, LABEL_AUTO);
    ips200pro_label_printf(label_reply_id,"云平台回复:");
    
    adc_init();

    // 初始化WiFi
    wifi_status = 0;
    time_get_data();                                                                                // 获取当前时间戳
    websocket_get_url(ASR_DOMAIN, asr_url_out);                                                     // 计算websocket_url，需要利用当前时间戳

    pit_init();                                                                                     // 开启麦克风接收定时器，频率为8K
    fifo_init(&adc_data_fifo, FIFO_DATA_16BIT, adc_get_data, (ASR_SEND_DATA_MAX_LENTH * 2));        // 初始化音频接收fifo

    pit_disable(ASR_PIT);                                                                           // 连接wifi前先关闭定时器
    
    // 此处不重复初始化WiFi，避免重复操作
    if (!wifi_status)
    {
        while(wifi_uart_init(ASR_WIFI_SSID, ASR_WIFI_PASSWORD, WIFI_UART_STATION))                      // 连接 WiFi 模块到指定的 WiFi 网络
        {
            printf("wifi连接失败，开始重连...\r\n");
            system_delay_ms(500);                                                                       // 初始化失败，等待 500ms
        }
        wifi_status = 1;
    }
    
    pit_enable(ASR_PIT);                                                                            // 连接wifi后打开定时器
    
    // 连接巴法云平台
    if (connect_bafa_cloud() == 0) {
        printf("\r\n巴法云平台连接成功\r\n");
        last_heartbeat_time = get_system_time_ms();
    } else {
        printf("\r\n巴法云平台连接失败，将在后台继续尝试连接\r\n");
    }
    
    ips200pro_label_show_string(label_text_id, "按下KEY4按键开始录音");
    printf("按下KEY4按键开始录音，录音后自动识别并发送到云平台\r\n");
}

// 处理语音识别结束
void handle_end_of_recognition(void)
{
    // 停止定时器
    pit_disable(ASR_PIT);
    
    // 语音数据结束帧数据包
    audio_data_send(2);
    // 发送结束帧数据包
    websocket_client_send((uint8_t*)json_data, strlen(json_data));
    // 等待语音结果
    system_delay_ms(1000);
    // 接收语音结果
    websocket_client_receive(websocket_receive_buffer);
    // 解析语音结果
    extract_content_fields((const char*)websocket_receive_buffer);

    if(asr_max_time > (60 * 8000))
    {
        printf("\r\n语音识别达到最大时长60秒\r\n");
        ips200pro_label_show_string(label_text_id, "语音识别达到最大时长60秒");
        asr_max_time = 0;
    }
    
    // 打印完整的识别结果
    printf("\r\n完整识别结果: %s\r\n", asr_result_buffer);
    
    // 断开语音识别连接
    websocket_client_close();
    system_delay_ms(500);
    
    // 重新初始化WiFi
    while(wifi_uart_init(ASR_WIFI_SSID, ASR_WIFI_PASSWORD, WIFI_UART_STATION))
    {
        system_delay_ms(500); // 初始化失败，等待 500ms
    }
    wifi_status = 1;
    
    // 重新启动定时器
    pit_enable(ASR_PIT);
    
    // 语音识别完成
    printf("\r\n语音识别处理完成\r\n");
    audio_get_count = -1;
    audio_need_net_flag = 0;
}

// 语音识别处理
void audio_loop()
{
    // 处理按键输入
    handle_button_press();
    
    // 根据当前状态进行处理
    if (current_state == STATE_IDLE) {
        process_idle_state();
    } else if (current_state == STATE_RECORDING) {
        process_recording_state();
    }
    
    // 处理语音识别连接
    if(audio_start_flag && !audio_server_link_flag)
    {
        printf("正在连接服务器...\r\n");
        
        // 在连接讯飞服务器前，暂时断开巴法云连接以释放资源
        disconnect_bafa_temporarily();
        
        // 确保WiFi状态正常
        check_and_restore_wifi();
        
        // 重新获取时间戳和计算URL
        time_get_data();
        websocket_get_url(ASR_DOMAIN, asr_url_out);
        
        // 连接服务器
        uint8 connect_retry = 0;
        while(!websocket_client_connect(asr_url_out))
        {
            printf("服务器连接失败，重试中...\r\n");
            system_delay_ms(1000); // 等待时间增加
            
            // 检查WiFi状态
            if (check_and_restore_wifi())
            {
                // 如果WiFi重连了，重新获取时间戳和URL
                time_get_data();
                websocket_get_url(ASR_DOMAIN, asr_url_out);
            }
            
            connect_retry++;
            if (connect_retry >= 5)
            {
                printf("服务器连接多次失败，请检查网络或稍后再试\r\n");
                ips200pro_label_show_string(label_text_id, "服务器连接失败，请检查网络或稍后再试");
                audio_start_flag = 0;
                audio_get_count = -1;
                current_state = STATE_IDLE;
                return;
            }
        }
        
        // 语音数据第一帧数据包
        audio_data_send(0);
        // 发送第一帧数据包
        websocket_client_send((uint8_t*)json_data, strlen(json_data));
        audio_server_link_flag = 1;
        printf("服务器连接成功，开始识别语音，最长识别时长为60秒，可手动按键停止\r\n");
        ips200pro_label_show_string(label_text_id, "服务器连接成功，开始识别语音");	// 设置文本标签文本信息
        
        memset(asr_textout, 0, sizeof(asr_textout));
        memset(asr_result_buffer, 0, sizeof(asr_result_buffer));
        strncpy((char*)asr_result_buffer, "语音识别结果:", strlen("语音识别结果:"));
        ips200pro_label_show_string(label_out_id, (char*)asr_result_buffer);	// 设置文本标签文本信息
        printf("语音识别结果:");
    }
    
    // 处理语音识别数据发送
    if(audio_start_flag && audio_server_link_flag && audio_send_data_flag)
    {
        audio_send_data_flag = 0;
        // 语音数据中间帧数据包
        audio_data_send(1);
        // 发送中间帧数据包
        websocket_client_send((uint8_t*)json_data, strlen(json_data));
        // 等待语音结果
        system_delay_ms(300);
        // 接收语音结果
        websocket_client_receive(websocket_receive_buffer);
        // 解析语音结果
        extract_content_fields((const char*)websocket_receive_buffer);
    }
    
    // 处理语音识别结束
    if(audio_need_net_flag && !audio_send_data_flag)
    {
        handle_end_of_recognition();
    }
}

// 按键处理函数
void handle_button_press(void)
{
    // 检测按键是否按下
    if(gpio_get_level(ASR_BUTTON) == 0 && !button_pressed)
    {
        button_pressed = 1; // 标记按键已按下，防止重复触发
        
        if(current_state == STATE_IDLE)
        {
            // 从空闲状态 -> 录音状态
            printf("\r\n开始录音...");
            ips200pro_label_show_string(label_text_id, "开始录音，检测到声音后开始识别");
            
            // 断开巴法云连接
            disconnect_bafa_temporarily();
            
            // 准备开始录音
            audio_get_count = 0;
            audio_server_link_flag = 0;
            audio_start_flag = 1;
            asr_max_time = 0;
            silence_counter = 0;
            is_speaking = 0;
            auto_end_flag = 0;
            new_result_available = 0;
            memset(asr_result_buffer, 0, sizeof(asr_result_buffer));
            
            current_state = STATE_RECORDING;
        }
        else if(current_state == STATE_RECORDING)
        {
            // 录音状态 -> 空闲状态（手动停止录音）
            printf("\r\n手动停止录音");
            ips200pro_label_show_string(label_text_id, "手动停止录音，处理中...");
            
            // 停止录音
            audio_start_flag = 0;
            audio_get_count = 0;
            audio_send_data_flag = 0;
            audio_need_net_flag = 1;
        }
    }
    else if(gpio_get_level(ASR_BUTTON) != 0 && button_pressed)
    {
        // 按键释放
        button_pressed = 0;
    }
}


// 处理空闲状态 - 增加持续监听巴法云消息的功能
void process_idle_state(void)
{
    uint32 current_time = get_system_time_ms();
    static uint32 last_reconnect_time = 0;
    static uint32 last_check_time = 0;
    static uint32 last_message_time = 0;
    static uint32 last_subscription_refresh = 0;
    static uint8 consecutive_empty_responses = 0;
    static uint8 heartbeat_response_received = 1; // 初始假设心跳响应正常
    
    // 检查WiFi连接状态
    if (current_time - last_check_time >= CONNECTION_CHECK_INTERVAL) { // 每60秒检查一次WiFi状态
        if (check_and_restore_wifi()) {
            // WiFi状态已恢复，重置巴法云连接
            bafa_connected = 0;
            heartbeat_response_received = 1; // 重置心跳响应标志
        }
        last_check_time = current_time;
    }
    
    // 空闲状态：维持巴法云连接
    if (bafa_connected) {
        // 心跳包处理
        if (current_time - last_heartbeat_time >= HEARTBEAT_INTERVAL) {
            // 如果上一次心跳没有收到响应，认为连接可能已断开
            if (!heartbeat_response_received) {
                printf("\r\n未收到上一次心跳响应，重新连接巴法云");
                bafa_connected = 0;
                last_reconnect_time = current_time - RECONNECT_INTERVAL; // 立即尝试重连
            } else {
                send_heartbeat();
                last_heartbeat_time = current_time;
                heartbeat_response_received = 0; // 重置心跳响应标志，等待响应
            }
        }
        
        // 定期刷新订阅，避免订阅失效
        if (current_time - last_subscription_refresh >= SUBSCRIPTION_REFRESH_INTERVAL) {
            printf("\r\n刷新巴法云订阅");
            // 发送订阅请求
            char subscribe_msg[128];
            sprintf(subscribe_msg, "cmd=1&uid=%s&topic=%s", BAFA_KEY, BAFA_TOPIC);
            wifi_uart_send_buffer((uint8_t *)subscribe_msg, strlen(subscribe_msg));
            last_subscription_refresh = current_time;
        }
        
        // 检查巴法云回复
        uint16 data_length = wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
        if (data_length > 0) {
            consecutive_empty_responses = 0; // 收到非空回复，重置计数
            last_message_time = current_time; // 更新最后消息时间
            
            // 检查是否收到心跳响应
            if (strstr((char*)wifi_uart_get_data_buffer, "cmd=0&res=1")) {
                heartbeat_response_received = 1; // 标记已收到心跳响应
            }
            // 检查是否收到订阅响应
            else if (strstr((char*)wifi_uart_get_data_buffer, "cmd=1&res=1")) {
                printf("\r\n订阅刷新成功");
            }
            
            check_bafa_reply();
        } else {
            consecutive_empty_responses++;
            
            // 如果连续多次无响应且间隔较长，考虑检查连接
            if (consecutive_empty_responses > 10 && current_time - last_message_time > 60000) { // 连续10次无响应且超过1分钟
                printf("\r\n连续多次无响应，检查连接状态");
                
                // 发送一次心跳包测试连接
                send_heartbeat();
                system_delay_ms(500); // 等待响应
                
                // 读取响应
                data_length = wifi_uart_read_buffer(wifi_uart_get_data_buffer, sizeof(wifi_uart_get_data_buffer));
                if (data_length == 0 || !strstr((char*)wifi_uart_get_data_buffer, "cmd=0&res=1")) {
                    // 未收到正确响应，认为连接已断开
                    printf("\r\n心跳测试失败，重新连接巴法云");
                    bafa_connected = 0;
                    consecutive_empty_responses = 0;
                } else {
                    // 收到响应，连接正常
                    printf("\r\n心跳测试成功，连接正常");
                    consecutive_empty_responses = 0;
                    last_message_time = current_time;
                    heartbeat_response_received = 1;
                }
            }
        }
    } else {
        // 如果没连接，尝试连接巴法云
        if (current_time - last_reconnect_time >= RECONNECT_INTERVAL) {
            printf("\r\n尝试连接巴法云...");
            if (connect_bafa_cloud() == 0) {
                last_heartbeat_time = current_time;
                last_message_time = current_time;
                last_subscription_refresh = current_time; // 重置订阅刷新计时器
                consecutive_empty_responses = 0;
                heartbeat_response_received = 1;
                printf("\r\n巴法云连接成功");
            } else {
                printf("\r\n巴法云连接失败，将在%d秒后重试", RECONNECT_INTERVAL/1000);
            }
            last_reconnect_time = current_time;
        }
    }
    
    // 增加短暂延时，避免CPU占用过高
    system_delay_ms(20);
}

// 处理录音状态 - 修改以支持TTS
void process_recording_state(void)
{
    // 检查是否需要自动结束录音
    if (auto_end_flag) {
        // 自动结束录音（静音检测）
        printf("\r\n检测到静音，自动停止录音");
        ips200pro_label_show_string(label_text_id, "检测到静音，自动停止录音");
        
        // 停止录音
        audio_start_flag = 0;
        audio_get_count = 0;
        audio_send_data_flag = 0;
        audio_need_net_flag = 1;
        auto_end_flag = 0;
    }
    
    // 如果录音结束，处理识别结果
    if (!audio_start_flag && audio_get_count == -1 && !audio_need_net_flag) {
        // 语音识别处理完成，发送结果到巴法云
        if (strlen((char*)asr_result_buffer) > strlen("语音识别结果:")) {
            // 连接巴法云
            printf("\r\n语音识别完成，连接巴法云发送结果");
            ips200pro_label_show_string(label_text_id, "语音识别完成，发送结果到云平台");
            
            // 尝试连接巴法云
            if (!bafa_connected) {
                connect_bafa_cloud();
            }
            
            if (bafa_connected) {
                // 跳过前缀"语音识别结果:"
                char* result_text = (char*)asr_result_buffer + strlen("语音识别结果:");
                
                // 添加STT前缀并发送到巴法云
                char stt_message[2048] = {0};
                strcpy(stt_message, STT_CMD_PREFIX);
                strcat(stt_message, result_text);
                send_message_to_bafa(stt_message);
                
                // 清空巴法云回复
                memset(bafa_cloud_reply, 0, sizeof(bafa_cloud_reply));
                ips200pro_label_show_string(label_reply_id, "等待云平台回复...");
            } else {
                // 连接失败，提示用户
                printf("\r\n连接巴法云失败，无法发送结果");
                ips200pro_label_show_string(label_text_id, "连接云平台失败，请检查网络");
            }
        } else {
            // 没有有效识别结果，提示用户
            printf("\r\n没有有效的语音识别结果");
            ips200pro_label_show_string(label_text_id, "没有有效的语音识别结果");
        }
        
        // 重置状态
        current_state = STATE_IDLE;
    }
}
