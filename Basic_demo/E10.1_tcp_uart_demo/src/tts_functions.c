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
* 文件名称          tts_functions
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
#include "tts_functions.h"
#include "utf-gbk/Utf8ToGbk.h"  // 添加UTF-8转GB2312的头文件

// TTS功能相关变量
uint8_t tts_response_buffer[256]; // TTS回复缓冲区
uint16_t tts_response_len = 0;    // TTS回复长度
uint8_t tts_response_ready = 0;   // TTS回复就绪标志
uint8_t tts_playing = 0;          // TTS播放状态：0-空闲，1-正在播放
uint32_t tts_response_count = 0;  // TTS回复计数器，用于区分开始和结束回复

// TTS消息队列
TTSQueue tts_queue = {0};        // TTS消息队列

// 分段消息处理相关变量
SegmentedMessage incoming_tts_message = {0}; // 接收到的TTS消息

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
void process_tts_queue(void) {
    // 如果TTS模块空闲且队列不为空，则处理下一条消息
    if (!tts_playing && !tts_queue_is_empty()) {
        char message[256];
        uint16_t length;
        
        if (tts_queue_dequeue(message, &length)) {
            printf("\r\n从队列中取出TTS消息，长度: %d", length);
            // 将UTF-8文本转换为GB2312后发送到TTS模块
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
    send_text_to_tts(gb2312_text, gb2312_len);
    
    printf("\r\n已将UTF-8文本转换为GB2312编码并发送到TTS模块");
    printf("\r\n原文本(UTF-8): ");
    for (uint16 i = 0; i < utf8_len; i++) {
        printf("%c", utf8_text[i]);
    }
    printf("\r\n");
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
    
    // 查找消息内容 - 查找"msg="后面的内容
    const char* msg_part = strstr(response, "msg=");
    if (msg_part == NULL) {
        printf("\r\n未找到msg=参数");
        return;
    }
    
    // 移动到msg=后面
    msg_part += 4;
    
    // 查找消息结束位置（&或结尾）
    const char* msg_end = strstr(msg_part, "&");
    if (msg_end == NULL) {
        msg_end = msg_part + strlen(msg_part);
    }
    
    // 计算消息长度并复制
    size_t msg_len = msg_end - msg_part;
    char temp_response[256];
    if (msg_len < sizeof(temp_response) - 1) {
        memcpy(temp_response, msg_part, msg_len);
        temp_response[msg_len] = '\0';
    } else {
        memcpy(temp_response, msg_part, sizeof(temp_response) - 1);
        temp_response[sizeof(temp_response) - 1] = '\0';
        msg_len = sizeof(temp_response) - 1;
    }
    
    printf("\r\n提取的消息内容: [%s]", temp_response);
    
    char prefix[16] = {0};
    uint8_t current_seg = 0;
    uint8_t total_segs = 0;
    char content[256] = {0};
    uint16_t content_len = 0;
    
    // 尝试解析分段消息格式
    if (parse_segmented_message(temp_response, msg_len, prefix, &current_seg, &total_segs, content, &content_len)) {
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
            
            // 将消息添加到队列
            tts_queue_enqueue(content, content_len);
            
            // 如果TTS模块空闲，尝试处理队列
            process_tts_queue();
        }
    } else {
        // 尝试检查是否是普通的TTS命令
        if (msg_len > TTS_CMD_PREFIX_LEN && starts_with(temp_response, TTS_CMD_PREFIX, TTS_CMD_PREFIX_LEN)) {
            printf("\r\n直接检测到TTS命令");
            const char *text_content = temp_response + TTS_CMD_PREFIX_LEN;
            uint16_t text_len = msg_len - TTS_CMD_PREFIX_LEN;
            
            printf("\r\n提取的纯文本内容: [%.*s]", text_len, text_content);
            
            // 将消息添加到队列
            tts_queue_enqueue(text_content, text_len);
            
            // 如果TTS模块空闲，尝试处理队列
            process_tts_queue();
        } else {
            printf("\r\n未识别的消息格式: [%s]", temp_response);
        }
    }
}

// 处理TTS消息，检查是否是TTS命令
void process_tts_message(const uint8 *text, uint16 len)
{
    // 检查是否是TTS命令
    if(len > TTS_CMD_PREFIX_LEN && starts_with((const char*)text, TTS_CMD_PREFIX, TTS_CMD_PREFIX_LEN)) {
        // 是TTS命令，将UTF-8文本转换为GB2312并发送给TTS模块
        const uint8 *content = &text[TTS_CMD_PREFIX_LEN];
        uint16 content_len = len - TTS_CMD_PREFIX_LEN;
        
        // 将消息添加到队列
        tts_queue_enqueue((const char*)content, content_len);
        
        // 如果TTS模块空闲，尝试处理队列
        process_tts_queue();
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
uint16 tts_read_data(uint8 *buffer, uint16 len)
{
    uint16 read_len = 0;
    
    while (uart3_rx_head != uart3_rx_tail && read_len < len) {
        buffer[read_len++] = uart3_rx_buf[uart3_rx_tail];
        uart3_rx_tail = (uart3_rx_tail + 1) % UART3_RX_BUF_SIZE;
    }
    
    return read_len;
}

// 初始化TTS功能
void tts_init(void)
{
    // 初始化变量
    tts_playing = 0;
    tts_response_ready = 0;
    tts_response_len = 0;
    tts_response_count = 0;
    
    // 初始化消息队列
    tts_queue.head = 0;
    tts_queue.tail = 0;
    tts_queue.count = 0;
    
    // 初始化分段消息结构
    memset(&incoming_tts_message, 0, sizeof(incoming_tts_message));
    
    printf("\r\nTTS功能初始化完成");
} 