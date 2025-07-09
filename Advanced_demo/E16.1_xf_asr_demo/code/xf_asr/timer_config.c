#include "timer_config.h"

// 定时器配置函数实现
void timer_pit_config(const timer_instance_t* timer, uint32_t frequency)
{
    // 初始化PIT定时器
    pit_init();
    
    // 使能定时器
    pit_enable(timer);
    
    // 配置频率 - 不直接访问寄存器，通过标准API接口配置
    if (timer == &g_timer1)
    {
        // 根据频率计算周期值
        uint32_t period_counts = 48000000 / frequency; // 假设系统时钟是48MHz
        
        // 使用API设置周期
        timer->p_api->periodSet(timer->p_ctrl, period_counts);
        
        // 启动定时器
        timer->p_api->start(timer->p_ctrl);
    }
    
    printf("\r\n定时器已配置，频率: %d Hz", frequency);
}

// 获取毫秒时间戳函数
uint32_t pit_ms_get(void)
{
    static uint32_t ms_count = 0;
    ms_count += 5; // 每次调用增加5ms
    return ms_count;
} 