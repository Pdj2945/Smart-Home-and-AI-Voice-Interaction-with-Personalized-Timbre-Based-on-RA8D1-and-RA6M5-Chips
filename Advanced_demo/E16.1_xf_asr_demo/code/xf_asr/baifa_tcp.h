#ifndef CODE_BAIFA_TCP_H_
#define CODE_BAIFA_TCP_H_

#include "zf_common_headfile.h"

// 巴法云TCP云平台配置
#define BAFA_HOST           "bemfa.com"
#define BAFA_PORT           "8344"
#define BAFA_KEY            "11ca31c562d14ef786a0238a03a3be16"  // 巴法云用户密钥
#define BAFA_TOPIC          "text1"                            // 订阅的主题
#define HEARTBEAT_INTERVAL  60                                 // 心跳间隔（秒）

// 连接状态全局变量声明
extern uint8_t baifa_connected;

// 获取毫秒时间戳函数
uint32_t pit_ms_get(void);

// 巴法云TCP客户端初始化
uint8_t baifa_tcp_init(void);

// 向巴法云发送消息
uint8_t baifa_tcp_send_message(const char* message);

// 巴法云TCP客户端处理函数，需要周期性调用
void baifa_tcp_process(void);

// 发送心跳包
void baifa_tcp_send_heartbeat(void);

// 获取是否已经连接到巴法云
uint8_t baifa_tcp_is_connected(void);

// 获取新消息
uint8_t baifa_tcp_get_new_message(char* buffer, uint16_t buffer_size);

#endif /* CODE_BAIFA_TCP_H_ */ 