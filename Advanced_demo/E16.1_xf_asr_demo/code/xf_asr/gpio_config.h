#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include "zf_common_headfile.h"

// 简化的GPIO配置枚举
typedef enum {
    GPI,        // 输入模式
    GPO         // 输出模式
} gpio_mode_enum;

// 简化的GPIO电平枚举
typedef enum {
    GPIO_LOW,   // 低电平
    GPIO_HIGH   // 高电平
} gpio_level_enum;

// 简化的GPIO上拉下拉枚举
typedef enum {
    GPI_FLOATING,   // 浮空输入
    GPI_PULL_DOWN,  // 下拉输入
    GPI_PULL_UP     // 上拉输入
} gpio_pull_enum;

// GPIO初始化函数
void gpio_init(uint32_t pin, gpio_mode_enum mode, gpio_level_enum level, gpio_pull_enum pull);

// 定义GPIO引脚名称数组
extern const char* gpio_pin_name[];

#endif // GPIO_CONFIG_H 