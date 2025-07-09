#ifndef SRC_TCP_UART_H_
#define SRC_TCP_UART_H_

#include "zf_common_headfile.h"

// 百法云TCP云平台配置
#define BAFA_HOST           "bemfa.com"              // 百法云服务器地址
#define BAFA_PORT           "8344"                   // 百法云端口号
#define BAFA_KEY            "11ca31c562d14ef786a0238a03a3be16"  // 百法云用户密钥
#define BAFA_TOPIC          "text3"                  // 订阅的主题
#define HEARTBEAT_INTERVAL  5000                     // 心跳间隔（毫秒）
#define PRINT_INTERVAL      60000                    // 状态打印间隔（毫秒）

// WiFi配置
#define WIFI_SSID           "Redmi K50 Pro"                    // WiFi名称
#define WIFI_PASSWORD       "pdj2945237036"              // WiFi密码

// 串口缓冲区定义
#define UART_FRAME_BUF_SIZE 1024                     // 串口帧缓冲区大小
// 注意：UART3_RX_BUF_SIZE已在zf_driver_uart.h中定义为256

// 连接状态全局变量声明
extern uint8_t tcp_connected;

// UWB相关结构体声明
typedef struct {
    uint32 message_header;  // 4字节 0xFFFFFFFF
    uint16 packet_length;   // 2字节 消息体长度
    uint16 sequence_id;     // 2字节 消息流水号
    uint16 request_command; // 2字节 命令号 0x2001为位置数据，0x2002为心跳包
    uint16 version_id;      // 2字节 版本号 固定为0x0100或0x0101
    uint32 anchor_id;       // 4字节 基站ID
    uint32 tag_id;          // 4字节 标签ID
    uint32 distance;        // 4字节 标签与基站间的距离，单位cm
    int16 azimuth;          // 2字节 标签与基站间的方位角，单位度
    int16 elevation;        // 2字节 标签与基站间的俯角，单位度
    uint16 tag_status;      // 2字节 标签的状态
    uint16 batch_sn;        // 2字节 测距序号
    uint32 reserve;         // 4字节 预留
    uint8 xor_byte;         // 1字节 校验字节
} UWB_Frame_t;

// 初始化TCP客户端
uint8_t tcp_uart_init(void);

// 向TCP服务器发送消息
uint8_t tcp_uart_send_message(const char* message);

// TCP客户端处理函数，需要周期性调用
void tcp_uart_process(void);

// 获取新消息
uint8_t tcp_uart_get_new_message(char* buffer, uint16_t buffer_size);

// 获取系统时间（毫秒）
uint32_t get_system_time_ms(void);

// 电机控制指令处理函数声明
void process_motor_command(const char* command);

// UWB数据解析相关函数声明
uint16_t read_uint16_big_endian(uint8_t *data);
uint32_t read_uint32_big_endian(uint8_t *data);
int16_t read_int16_big_endian(uint8_t *data);
void parse_uwb_frame(uint8_t *data, uint16_t length);

#endif /* SRC_TCP_UART_H_ */ 