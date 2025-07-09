/*********************************************************************************************************************
* RA8D1 Opensourec Library 即（RA8D1 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2025 SEEKFREE 逐飞科技
* 
* 本文件是 RA8D1 开源库的一部分
* 
* RA8D1 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
* 
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
* 
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
* 
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
* 
* 文件名称          hal_entry
* 公司名称          成都逐飞科技有限公司
* 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
* 开发环境          MDK 5.38
* 适用平台          RA8D1
* 店铺链接          https://seekfree.taobao.com/
* 
* 修改记录
* 日期              作者                备注
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "zf_common_headfile.h"
#include "asr_audio.h"
#include "utf-gbk/Utf8ToGbk.h"

// TTS功能相关定义
#define TTS_CMD_PREFIX         "tts:"
#define TTS_CMD_PREFIX_LEN     4
#define STT_CMD_PREFIX         "stt:"
#define STT_CMD_PREFIX_LEN     4

// 分段消息相关定义
#define MAX_SEGMENT_SIZE       50    // 每段最大字符数
#define MAX_SEGMENTS           10    // 最大段数

// TTS消息队列定义
#define TTS_QUEUE_SIZE         20    // 队列大小，最多缓存20条消息

// TTS功能相关变量 - 自定义UART3接收缓冲区
#define UART3_RX_BUF_SIZE 256
volatile uint8_t uart3_rx_buf[UART3_RX_BUF_SIZE];  // UART3接收缓冲区
volatile uint16_t uart3_rx_head = 0; // 接收缓冲区头指针
volatile uint16_t uart3_rx_tail = 0; // 接收缓冲区尾指针

// TTS模块回复处理相关变量
uint8_t tts_response_buffer[256]; // TTS回复缓冲区
uint16_t tts_response_len = 0;    // TTS回复长度
uint8_t tts_response_ready = 0;   // TTS回复就绪标志
uint8_t tts_playing = 0;          // TTS播放状态：0-空闲，1-正在播放
uint32_t tts_response_count = 0;  // TTS回复计数器，用于区分开始和结束回复
uint8_t tts_segment_counter = 0;  // TTS段计数器，用于跟踪当前处理的段号

// TTS消息队列
typedef struct {
    char message[256];           // 消息内容
    uint16_t length;             // 消息长度
} TTSMessage;

typedef struct {
    TTSMessage messages[TTS_QUEUE_SIZE];  // 消息数组
    uint8_t head;                         // 队列头指针
    uint8_t tail;                         // 队列尾指针
    uint8_t count;                        // 队列中的消息数量
} TTSQueue;

TTSQueue tts_queue = {0};        // TTS消息队列

// 分段消息处理相关变量
typedef struct {
    uint8_t current_segment;      // 当前段索引
    uint8_t total_segments;       // 总段数
    uint8_t is_complete;          // 是否接收完成
    char    message[MAX_SEGMENTS * MAX_SEGMENT_SIZE]; // 完整消息缓冲区
    uint16_t message_len;         // 完整消息长度
} SegmentedMessage;

SegmentedMessage incoming_tts_message = {0}; // 接收到的TTS消息
SegmentedMessage outgoing_stt_message = {0}; // 待发送的STT消息

// 声明UART3回调函数 - 库文件中已经实现，这里只是声明
extern void uart3_callback(uart_callback_args_t* p_args);

// TTS消息队列操作函数
bool tts_queue_is_empty(void) {
    return tts_queue.count == 0;
}

bool tts_queue_is_full(void) {
    return tts_queue.count >= TTS_QUEUE_SIZE;
}

// 添加消息到队列
bool tts_queue_enqueue(const char* message, uint16_t length) {
    if (tts_queue_is_full() || length >= sizeof(tts_queue.messages[0].message)) {
        printf("\r\nTTS队列已满或消息过长，无法添加");
        return false;
    }
    
    // 检查是否是新的TTS消息的第一段
    // 如果是分段消息的第一段，或者是新的单段消息，则重置段计数器
    if (incoming_tts_message.current_segment == 1 || 
        (incoming_tts_message.current_segment == 0 && incoming_tts_message.total_segments == 0)) {
        tts_segment_counter = 1;
    } else {
        // 否则递增段计数器
        tts_segment_counter++;
    }
    
    printf("\r\nTTS段计数器: %d", tts_segment_counter);
    
    // 复制消息到队列
    memset(tts_queue.messages[tts_queue.tail].message, 0, sizeof(tts_queue.messages[0].message));
    memcpy(tts_queue.messages[tts_queue.tail].message, message, length);
    tts_queue.messages[tts_queue.tail].length = length;
    
    // 更新队列尾指针
    tts_queue.tail = (tts_queue.tail + 1) % TTS_QUEUE_SIZE;
    tts_queue.count++;
    
    printf("\r\n消息已添加到TTS队列，当前队列长度: %d", tts_queue.count);
    return true;
}

// 从队列获取消息
bool tts_queue_dequeue(char* message, uint16_t* length) {
    if (tts_queue_is_empty()) {
        return false;
    }
    
    // 复制队列头的消息
    memcpy(message, tts_queue.messages[tts_queue.head].message, tts_queue.messages[tts_queue.head].length);
    *length = tts_queue.messages[tts_queue.head].length;
    
    // 更新队列头指针
    tts_queue.head = (tts_queue.head + 1) % TTS_QUEUE_SIZE;
    tts_queue.count--;
    
    printf("\r\n从TTS队列取出消息，当前队列长度: %d", tts_queue.count);
    return true;
}

// 处理TTS队列
void process_tts_queue(void)
{
    // 如果TTS模块空闲且队列不为空，则处理下一条消息
    if (!tts_playing && !tts_queue_is_empty()) {
        char message[256];
        uint16_t length;
        
        if (tts_queue_dequeue(message, &length)) {
            printf("\r\n从队列中取出TTS消息，长度: %d", length);
            
            // 发送TTS消息前，更新屏幕显示当前段的内容
            // 注意：这里只更新TTS回复标签，不影响语音识别结果标签
            ips200pro_label_show_string(label_reply_id, message);
            
            // 发送消息到TTS模块
            send_utf8_text_to_tts((uint8*)message, length);
        }
    }
}

// 检查字符串是否以指定前缀开始
bool starts_with(const char* str, const char* prefix, int prefix_len) {
    for (int i = 0; i < prefix_len; i++) {
        if (str[i] != prefix[i]) {
            return false;
        }
    }
    return true;
}

// 发送文本到TTS模块
void send_text_to_tts(const uint8 *text, uint16 len) 
{
    // 发送数据到UART3
    uart3_write_buffer(text, len);
    printf("\r\n发送到TTS模块: ");
    for (uint16 i = 0; i < len; i++) {
        printf("%c", text[i]);
    }
    printf("\r\n");
    
    // 设置TTS正在播放状态
    tts_playing = 1;
    tts_response_count = 0;
}

// 发送UTF-8文本到TTS模块（转换为GB2312编码）
void send_utf8_text_to_tts(const uint8 *utf8_text, uint16 utf8_len) 
{
    uint8 gb2312_text[256]; // 转换后的GB2312文本缓冲区
    int gb2312_len = 0;
    
    // 使用库函数将UTF-8转换为GB2312
    SwitchToGbk(utf8_text, utf8_len, gb2312_text, &gb2312_len);
    
    // 确保缓冲区不会溢出
    if (gb2312_len > 0 && gb2312_len < 255) {
        // 添加结束字符，确保TTS模块正确识别字符串结束
        gb2312_text[gb2312_len] = 0x00;  // 添加NULL结束符
        gb2312_len++;  // 增加长度以包含结束符
    }
    
    // 发送转换后的GB2312文本到TTS模块
    uart3_write_buffer(gb2312_text, gb2312_len);
    
    printf("\r\npnbufoutlen=%d\r\n", gb2312_len);
    printf("\r\n已将UTF-8文本转换为GB2312编码并发送到TTS模块");
    printf("\r\n原文本(UTF-8): ");
    for (uint16 i = 0; i < utf8_len; i++) {
        printf("%c", utf8_text[i]);
    }
    printf("\r\n");
    
    // 设置TTS正在播放状态
    tts_playing = 1;
    tts_response_count = 0;
}

// 解析分段消息格式 tts(m|n):文本内容
bool parse_segmented_message(const char *text, uint16_t len, char *prefix, uint8_t *current_seg, uint8_t *total_segs, char *content, uint16_t *content_len)
{
    // 最小格式: "xxx(1|1):"，至少需要8个字符
    if (len < 8) {
        printf("\r\n消息太短，无法解析分段格式: %d < 8", len);
        return false;
    }
    
    // 检查前缀
    int prefix_len = 0;
    while (prefix_len < len && text[prefix_len] != '(' && prefix_len < 10) {
        prefix[prefix_len] = text[prefix_len];
        prefix_len++;
    }
    prefix[prefix_len] = '\0';
    
    // 检查格式是否正确
    if (prefix_len >= len || text[prefix_len] != '(') {
        printf("\r\n未找到左括号，不是分段格式");
        return false;
    }
    
    printf("\r\n解析前缀: [%s]", prefix);
    
    // 解析当前段和总段数
    int pos = prefix_len + 1;
    char num_buf[4] = {0};
    int num_pos = 0;
    
    // 解析当前段
    while (pos < len && text[pos] != '|' && num_pos < 3) {
        num_buf[num_pos++] = text[pos++];
    }
    num_buf[num_pos] = '\0';
    *current_seg = atoi(num_buf);
    
    if (pos >= len || text[pos] != '|') {
        printf("\r\n未找到分隔符|，格式错误");
        return false;
    }
    pos++;
    
    // 解析总段数
    num_pos = 0;
    memset(num_buf, 0, sizeof(num_buf));
    while (pos < len && text[pos] != ')' && num_pos < 3) {
        num_buf[num_pos++] = text[pos++];
    }
    num_buf[num_pos] = '\0';
    *total_segs = atoi(num_buf);
    
    if (pos >= len || text[pos] != ')') {
        printf("\r\n未找到右括号，格式错误");
        return false;
    }
    pos++;
    
    // 检查冒号
    if (pos >= len || text[pos] != ':') {
        printf("\r\n未找到冒号，格式错误");
        return false;
    }
    pos++;
    
    // 提取内容
    *content_len = len - pos;
    memcpy(content, &text[pos], *content_len);
    content[*content_len] = '\0'; // 确保字符串结束
    
    printf("\r\n成功解析分段格式: 前缀=[%s], 段=%d/%d, 内容=[%s], 长度=%d", 
           prefix, *current_seg, *total_segs, content, *content_len);
    
    return true;
}

// 处理巴法云回复的消息
void process_cloud_response(const char *response, uint16_t len)
{
    printf("\r\n处理巴法云回复");
    printf("\r\n解析出的回复内容: [%.*s]", len, response);
    
    // 创建一个临时副本，确保字符串结束
    char temp_response[256];
    if (len < sizeof(temp_response) - 1) {
        memcpy(temp_response, response, len);
        temp_response[len] = '\0';
    } else {
        memcpy(temp_response, response, sizeof(temp_response) - 1);
        temp_response[sizeof(temp_response) - 1] = '\0';
        len = sizeof(temp_response) - 1;
    }
    
    char prefix[16] = {0};
    uint8_t current_seg = 0;
    uint8_t total_segs = 0;
    char content[256] = {0};
    uint16_t content_len = 0;
    
    // 尝试解析分段消息格式
    if (parse_segmented_message(temp_response, len, prefix, &current_seg, &total_segs, content, &content_len)) {
        printf("\r\n成功解析分段消息: 前缀=%s, 段=%d/%d, 内容长度=%d", 
               prefix, current_seg, total_segs, content_len);
        
        // 检查是否是TTS消息
        if (strcmp(prefix, "tts") == 0) {
            printf("\r\n处理分段TTS消息");
            
            // 记录当前段信息，用于跟踪分段状态
            incoming_tts_message.current_segment = current_seg;
            incoming_tts_message.total_segments = total_segs;
            
            // 直接处理当前段内容，不再合并
            printf("\r\n直接处理TTS分段内容: 段%d/%d, 内容=[%s], 长度=%d", 
                   current_seg, total_segs, content, content_len);
            
            // 在屏幕上显示纯文本内容（不含前缀）
            // 每次显示新段时清除上一段内容
            if (current_seg == 1) {
                // 第一段，清除上一条TTS消息
                printf("\r\n收到新的TTS消息第一段，清除上一条消息");
            }
            
            // 显示当前段内容
            ips200pro_label_show_string(label_reply_id, content);
            
            // 将消息添加到队列
            tts_queue_enqueue(content, content_len);
            
            // 如果TTS模块空闲，尝试处理队列
            process_tts_queue();
        } else {
            // 非TTS消息，不显示到屏幕上
            printf("\r\n非TTS分段消息，不显示到屏幕上");
        }
    } else {
        // 尝试检查是否是普通的TTS命令
        if (len > TTS_CMD_PREFIX_LEN && starts_with(temp_response, TTS_CMD_PREFIX, TTS_CMD_PREFIX_LEN)) {
            printf("\r\n直接检测到TTS命令");
            const char *text_content = temp_response + TTS_CMD_PREFIX_LEN;
            uint16_t text_len = len - TTS_CMD_PREFIX_LEN;
            
            printf("\r\n提取的纯文本内容: [%.*s]", text_len, text_content);
            printf("\r\n构建的TTS命令: [%.*s]", len, temp_response);
            
            // 在屏幕上显示纯文本内容（不含前缀）
            char display_text[256] = {0};
            if (text_len < sizeof(display_text)) {
                strncpy(display_text, text_content, text_len);
                display_text[text_len] = '\0';
                
                // 清除上一条TTS消息并显示新消息
                // 这是一个新的TTS消息，重置段计数器
                incoming_tts_message.current_segment = 0;
                incoming_tts_message.total_segments = 0;
                
                ips200pro_label_show_string(label_reply_id, display_text);
            }
            
            // 将消息添加到队列
            tts_queue_enqueue(text_content, text_len);
            
            // 如果TTS模块空闲，尝试处理队列
            process_tts_queue();
        } else {
            printf("\r\n未识别的消息格式或非TTS消息，不显示到屏幕上: [%.*s]", len, temp_response);
        }
    }
}

// 准备分段STT消息
void prepare_segmented_stt_message(const char *text, uint16_t len)
{
    // 初始化分段消息结构
    outgoing_stt_message.current_segment = 1;
    outgoing_stt_message.total_segments = (len + MAX_SEGMENT_SIZE - 1) / MAX_SEGMENT_SIZE; // 向上取整
    outgoing_stt_message.is_complete = 0;
    outgoing_stt_message.message_len = len;
    
    // 复制消息内容
    if (len < sizeof(outgoing_stt_message.message)) {
        memcpy(outgoing_stt_message.message, text, len);
        outgoing_stt_message.message[len] = '\0';
    } else {
        memcpy(outgoing_stt_message.message, text, sizeof(outgoing_stt_message.message) - 1);
        outgoing_stt_message.message[sizeof(outgoing_stt_message.message) - 1] = '\0';
        outgoing_stt_message.message_len = sizeof(outgoing_stt_message.message) - 1;
    }
    
    printf("\r\n准备发送分段STT消息，总长度: %d, 分为%d段", outgoing_stt_message.message_len, outgoing_stt_message.total_segments);
}

// 获取下一段STT消息
bool get_next_stt_segment(char *buffer, uint16_t buffer_size, uint16_t *segment_len)
{
    if (outgoing_stt_message.current_segment > outgoing_stt_message.total_segments) {
        return false; // 已发送完所有段
    }
    
    // 计算当前段的起始位置和长度
    uint16_t start_pos = (outgoing_stt_message.current_segment - 1) * MAX_SEGMENT_SIZE;
    uint16_t content_len = MAX_SEGMENT_SIZE;
    
    // 调整最后一段的长度
    if (outgoing_stt_message.current_segment == outgoing_stt_message.total_segments) {
        content_len = outgoing_stt_message.message_len - start_pos;
    }
    
    // 构造分段消息格式: stt(m|n):content
    snprintf(buffer, buffer_size, "stt(%d|%d):", 
             outgoing_stt_message.current_segment, 
             outgoing_stt_message.total_segments);
    
    uint16_t prefix_len = strlen(buffer);
    
    // 复制内容
    if (prefix_len + content_len < buffer_size) {
        memcpy(buffer + prefix_len, outgoing_stt_message.message + start_pos, content_len);
        buffer[prefix_len + content_len] = '\0';
        *segment_len = prefix_len + content_len;
        
        // 更新段索引
        outgoing_stt_message.current_segment++;
        
        // 检查是否发送完成
        if (outgoing_stt_message.current_segment > outgoing_stt_message.total_segments) {
            outgoing_stt_message.is_complete = 1;
        }
        
        return true;
    }
    
    return false;
}

// 处理TTS消息，检查是否是TTS命令
void process_tts_message(const uint8 *text, uint16 len)
{
    // 检查是否是TTS命令
    if(len > TTS_CMD_PREFIX_LEN && starts_with((const char*)text, TTS_CMD_PREFIX, TTS_CMD_PREFIX_LEN)) {
        // 是TTS命令，将UTF-8文本转换为GB2312并发送给TTS模块
        send_utf8_text_to_tts(&text[TTS_CMD_PREFIX_LEN], len - TTS_CMD_PREFIX_LEN);
    } else {
        // 非TTS命令，仅打印文本
        printf("\r\n收到普通文本: %s\r\n", text);
    }
}

// 处理TTS模块的回复
void process_tts_response(void)
{
    if (tts_response_ready) {
        // 处理TTS模块的回复
        printf("\r\nTTS模块回复: ");
        for (uint16 i = 0; i < tts_response_len && i < 30; i++) {
            printf("%02X ", tts_response_buffer[i]);
        }
        if (tts_response_len > 30) {
            printf("...");
        }
        printf("\r\n");
        
        // 如果回复包含文本，则打印文本内容
        if (tts_response_len > 0) {
            tts_response_buffer[tts_response_len] = '\0'; // 确保字符串结束
            printf("TTS模块回复文本: %s\r\n", tts_response_buffer);
        }
        
        // 更新TTS播放状态
        if (tts_playing) {
            tts_response_count++;
            if (tts_response_count >= 2) {
                // 第二次回复，表示TTS播放结束
                printf("\r\nTTS播放结束");
                tts_playing = 0;
                tts_response_count = 0;
                
                // TTS播放结束后，尝试处理队列中的下一条消息
                // 这样可以确保多段文字与语音同步，一段播放完后再播放下一段
                system_delay_ms(100); // 短暂延时，确保TTS模块完全就绪
                process_tts_queue();
            } else {
                printf("\r\nTTS开始播放");
            }
        }
        
        // 重置回复状态
        tts_response_ready = 0;
        tts_response_len = 0;
    }
}

// 从UART3接收缓冲区读取数据
uint16 uart3_read_data(uint8 *buffer, uint16 len)
{
    uint16 read_len = 0;
    
    while (uart3_rx_head != uart3_rx_tail && read_len < len) {
        buffer[read_len++] = uart3_rx_buf[uart3_rx_head];
        uart3_rx_head = (uart3_rx_head + 1) % UART3_RX_BUF_SIZE;
    }
    
    return read_len;
}

// 处理语音识别结果
void handle_asr_result(const char* result, uint16_t len)
{
    // 检查结果是否有效
    if (result && len > 0) {
        printf("\r\n语音识别结果: [%.*s], 长度: %d", len, result, len);
        
        // 如果文本较短，直接发送
        if (len <= MAX_SEGMENT_SIZE) {
            // 构造STT消息
            char stt_message[MAX_SEGMENT_SIZE + STT_CMD_PREFIX_LEN + 1];
            snprintf(stt_message, sizeof(stt_message), "%s%s", STT_CMD_PREFIX, result);
            
            // 发送到云平台
            printf("\r\n发送STT消息到云平台: [%s]", stt_message);
            // TODO: 调用发送到云平台的函数
        } else {
            // 长文本，需要分段发送
            prepare_segmented_stt_message(result, len);
            
            // 获取并发送第一段
            char segment_buffer[MAX_SEGMENT_SIZE * 2];
            uint16_t segment_len = 0;
            
            if (get_next_stt_segment(segment_buffer, sizeof(segment_buffer), &segment_len)) {
                printf("\r\n发送分段STT消息到云平台: [%s]", segment_buffer);
                // TODO: 调用发送到云平台的函数
            }
        }
    }
}

// *************************** 例程硬件连接说明 ***************************
// 使用 Wi-Fi 转串口模块
// 麦克风直接使用板上的麦克风
// *************************** 例程使用操作说明 ***************************
//1.参照硬件连接说明连接好模块，使用电源供电(供电不足会导致模块电压不稳)
//
//2.查看电脑连接的wifi并记录wifi名称，密码，IP地址，修改asr_ctrl.h中的WiFi名称和wifi密码
//
//3.查看讯飞的id，Secret，Key，修改asr_ctrl.h中的ASR_APIID，ASR_APISecret，ASR_APIKey
//
//4.上电或者复位后启动，打开电脑上位机的调试窗口，查一下单片机复位后输出的信息中，需要配置为位机中使用UTF-8编码

// *************************** 运行操作说明 ***************************
// 1.首先程序会通过 Debug 串口输出调试信息 请接好对应的调试串口以获取调试信息
//
// 2.程序会使用串口助手上位机打开对应的窗口，窗口波特率为 zf_common_debug.h 文件中 DEBUG_UART_BAUDRATE 宏定义 默认 115200
//
// 3.连接好模块后（具体使用哪个请参见程序最上方备注供电不足可能会导致问题） 按下复位键开始初始化
//
// 4.按下复位键位机会显示：等待初始化...，等初始化完成后会显示：按下KEY4开始连接服务器，连接成功后开始识别，开始识别后再次按下按键停止识别
//
// 5.按下KEY4键后位机会显示：正在连接服务器...，如果连接成功位机会显示：服务器连接成功开始识别，开始识别时间为60秒，或手动按键终止。
//
// 6.到时间或者按键后说明：位机会输出识别到的内容，最长识别60秒
//
// 7.如果需要停止识别再按一下按键即可，等待位机显示"语音识别完成"，再次按下按键"开始识别"，请不要再次按键进入下一次识别
//
void hal_entry(void)
{
    init_sdram();               // 初始化所有引脚和SDRAM  全局变量后添加BSP_PLACE_IN_SECTION(".sdram")，将变量转到SDRAM中                         
    debug_init();               // 初始化Debug串口
    system_delay_ms(300);       // 等待主板其他外设上电完成
    uart3_init();               // 初始化串口3 - 连接TTS模块
    audio_init();               // 初始化语音识别模块
    
    printf("\r\n================语音助手系统================");
    printf("\r\n集成功能：");
    printf("\r\n1.语音识别：按下KEY4开始录音，说话后自动识别");
    printf("\r\n2.文字转语音：收到tts:前缀的文本将转发到TTS模块");
    printf("\r\n3.云平台互动：识别结果添加stt:前缀发送到巴法云");
    printf("\r\n4.支持长文本分段处理：格式为stt(m|n):或tts(m|n):");
    printf("\r\n5.TTS消息队列：支持缓存多条消息，顺序播放");
    printf("\r\n============================================\r\n");
    
    while(1)
    {    
        audio_loop();
        
        // 检查TTS模块回复
        uint8_t temp_buffer[256];
        uint16_t read_len = uart3_read_data(temp_buffer, sizeof(temp_buffer));
        
        if (read_len > 0) {
            // 收到TTS模块的回复
            memcpy(tts_response_buffer, temp_buffer, read_len);
            tts_response_len = read_len;
            tts_response_ready = 1;
            
            // 处理TTS模块回复
            process_tts_response();
        }
    }
}

void agt1_callback(timer_callback_args_t* p_args)
{
    if(TIMER_EVENT_CYCLE_END == p_args->event)
    {
        audio_callback();
    }
}

// **************************** 代码区域 ****************************
