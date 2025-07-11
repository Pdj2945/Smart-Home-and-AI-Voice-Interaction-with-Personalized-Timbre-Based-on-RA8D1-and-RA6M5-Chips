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
* 文件名称          zf_driver_pit
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
#include "zf_driver_pit.h"

__attribute__((weak)) void agt0_callback(timer_callback_args_t* p_args)
{
    if(TIMER_EVENT_CYCLE_END == p_args->event)
    {
    }
}

__attribute__((weak)) void agt1_callback(timer_callback_args_t* p_args)
{
    if(TIMER_EVENT_CYCLE_END == p_args->event)
    {
    }
}

void pit_init (void)
{
    g_timer0.p_api->open(g_timer0.p_ctrl, g_timer0.p_cfg);  // 开启5ms定时器
    g_timer0.p_api->start(g_timer0.p_ctrl);  
    
    g_timer1.p_api->open(g_timer1.p_ctrl, g_timer1.p_cfg);  // 开启125us定时器
    g_timer1.p_api->start(g_timer1.p_ctrl);  
}

void pit_enable(const timer_instance_t* timer)
{
    timer->p_api->start(timer->p_ctrl);
}

void pit_disable(const timer_instance_t* timer)
{
    timer->p_api->stop(timer->p_ctrl);
}