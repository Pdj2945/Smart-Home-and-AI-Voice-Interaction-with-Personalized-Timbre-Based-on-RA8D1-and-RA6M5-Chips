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
#ifndef TTS_FUNCTIONS_H
#define TTS_FUNCTIONS_H

#include "zf_common_headfile.h"

// TTS功能相关定义
#define TTS_CMD_PREFIX         "tts:"
#define TTS_CMD_PREFIX_LEN     4

// 分段消息相关定义
#define MAX_SEGMENT_SIZE       50    // 每段最大字符数
#define MAX_SEGMENTS           10    // 最大段数

// TTS消息队列定义
#define TTS_QUEUE_SIZE         20    // 队列大小，最多缓存20条消息

// TTS功能相关变量 - 自定义UART3接收缓冲区
#define UART3_RX_BUF_SIZE      256   // UART3接收缓冲区大小

// TTS模块外部变量声明
extern volatile uint8_t uart3_rx_buf[];      // UART3接收缓冲区
extern volatile uint16_t uart3_rx_head;      // 接收缓冲区头指针
extern volatile uint16_t uart3_rx_tail;      // 接收缓冲区尾指针

// TTS模块相关变量声明
extern uint8_t tts_response_buffer[];        // TTS回复缓冲区
extern uint16_t tts_response_len;            // TTS回复长度
extern uint8_t tts_response_ready;           // TTS回复就绪标志
extern uint8_t tts_playing;                  // TTS播放状态：0-空闲，1-正在播放
extern uint32_t tts_response_count;          // TTS回复计数器，用于区分开始和结束回复

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

// 分段消息处理相关结构
typedef struct {
    uint8_t current_segment;      // 当前段索引
    uint8_t total_segments;       // 总段数
    uint8_t is_complete;          // 是否接收完成
    char    message[MAX_SEGMENTS * MAX_SEGMENT_SIZE]; // 完整消息缓冲区
    uint16_t message_len;         // 完整消息长度
} SegmentedMessage;

// TTS功能函数声明
// 队列操作函数
bool tts_queue_is_empty(void);
bool tts_queue_is_full(void);
bool tts_queue_enqueue(const char* message, uint16_t length);
bool tts_queue_dequeue(char* message, uint16_t* length);
void process_tts_queue(void);

// 字符串处理函数
bool starts_with(const char* str, const char* prefix, int prefix_len);

// TTS通信函数
void send_text_to_tts(const uint8 *text, uint16 len);
void send_utf8_text_to_tts(const uint8 *utf8_text, uint16 utf8_len);
bool parse_segmented_message(const char *text, uint16_t len, char *prefix, uint8_t *current_seg, uint8_t *total_segs, char *content, uint16_t *content_len);
void process_cloud_response(const char *response, uint16_t len);
void process_tts_message(const uint8 *text, uint16 len);
void process_tts_response(void);
uint16 tts_read_data(uint8 *buffer, uint16 len);

// TTS初始化函数
void tts_init(void);

#endif // TTS_FUNCTIONS_H 