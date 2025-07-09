#ifndef TIMER_CONFIG_H
#define TIMER_CONFIG_H

#include "zf_common_headfile.h"
#include "zf_driver_pit.h"

// 定时器配置函数
void timer_pit_config(const timer_instance_t* timer, uint32_t frequency);

// 获取毫秒时间戳函数
uint32_t pit_ms_get(void);

#endif // TIMER_CONFIG_H 