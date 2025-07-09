#ifndef CODE_ASR_CTRL_H_
#define CODE_ASR_CTRL_H_

#include "zf_common_headfile.h"

#define ASR_TARGET_IP           "ws-api.xfyun.cn"
#define ASR_TARGET_PORT         "80"
#define ASR_DOMAIN              "wss://ws-api.xfyun.cn/v2/iat"
#define ASR_BUTTON              KEY4
#define ASR_PIT                 &g_timer1

#define ASR_WIFI_SSID           "Redmi K50 Pro"                         // wifi名称 wifi需要是2.4G频率
#define ASR_WIFI_PASSWORD       "pdj2945237036"                            // wifi密码

#define ASR_APIID               "815f5e7c"                              // 讯飞的id
#define ASR_APISecret           "ZTA5YjcwOGM0M2Q0OWMxMTIzMzMxMWM4"      // 讯飞的Secret
#define ASR_APIKey              "17498d8575d77817635f0a9662abf64c"      // 讯飞的Key

// 巴法云平台配置
#define BAFA_SERVER            "bemfa.com"                              // 巴法云服务器地址
#define BAFA_PORT              "8344"                                   // 巴法云端口号
#define BAFA_TOPIC             "text1"                                  // 订阅的主题
#define BAFA_KEY               "11ca31c562d14ef786a0238a03a3be16"       // 用户密钥
#define HEARTBEAT_INTERVAL     20000                                    // 心跳间隔20秒（更频繁发送心跳）
#define RECONNECT_MAX_ATTEMPTS 10                                       // 最大重连次数
#define RECONNECT_INTERVAL     10000                                    // 重连间隔10秒
#define WIFI_READ_TIMEOUT      500                                      // WiFi读取超时时间（毫秒）
#define CONNECTION_CHECK_INTERVAL 60000                                 // 连接状态检查间隔60秒
#define SUBSCRIPTION_REFRESH_INTERVAL 300000                            // 订阅刷新间隔5分钟

extern uint16 label_text_id;											// 创建一个文本标签ID
extern uint16 label_out_id;											// 创建一个文本标签ID
extern uint16 label_reply_id;                                           // 创建一个用于显示云平台回复的文本标签

#endif /* CODE_ASR_CTRL_H_ */
