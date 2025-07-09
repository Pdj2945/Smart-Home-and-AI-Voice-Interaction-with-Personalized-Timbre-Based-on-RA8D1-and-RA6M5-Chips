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
* 文件名称          zf_driver_uart
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
#include "zf_driver_uart.h"
#include "zf_common_debug.h"

// uart3的信号量
static volatile bool uart3_send_complete_flag = false;
volatile uint8_t uart3_rx_buf[UART3_RX_BUF_SIZE];
volatile uint16_t uart3_rx_head = 0;
volatile uint16_t uart3_rx_tail = 0;

// uart9_callback函数已在zf_common_debug.c中定义，此处不再重复定义

void uart3_callback (uart_callback_args_t* p_args)
{
    switch(p_args->event)
    {
    case UART_EVENT_RX_CHAR:
    {
        uint16_t next_head = (uart3_rx_head + 1) % UART3_RX_BUF_SIZE;
        if (next_head != uart3_rx_tail) // 缓冲区未满
        {
            uart3_rx_buf[uart3_rx_head] = (uint8_t)p_args->data;
            uart3_rx_head = next_head;
        }
        break;
    }
    case UART_EVENT_TX_COMPLETE:
    {
        uart3_send_complete_flag = true;
        break;
    }
    default:
        break;
    }
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     输出字节串
// 参数说明     uart_n          串口模块号 参照 zf_driver_uart.h 中 uart_index_enum 枚举定义
// 参数说明     dat             需要发送的字节
// 返回参数     void        
// 使用示例     uart_write_byte(0xA5);                                  // 串口1发送0xA5
// 备注信息     
//-------------------------------------------------------------------------------------------------------------------
// UART9 写字节函数 - 使用debug_write_buffer
void uart9_write_byte (const uint8 dat)
{
    debug_write_buffer(&dat, 1);
}

void uart3_write_byte (const uint8 dat)
{
    uart3_send_complete_flag = false;
    g_uart3.p_api->write(g_uart3.p_ctrl, (const uint8*)&dat, 1);     // 发送单个字符
    while(!uart3_send_complete_flag);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     串口发送数组
// 参数说明     uart_n          串口模块号 参照 zf_driver_uart.h 中 uart_index_enum 枚举定义
// 参数说明     *buff           要发送的数组地址
// 参数说明     len             发送长度
// 返回参数     void
// 使用示例     uart_write_buffer(&a[0], 5);
// 备注信息     
//-------------------------------------------------------------------------------------------------------------------
// UART9 发送数据缓冲区 - 使用debug_write_buffer
void uart9_write_buffer (const uint8 *buff, uint32 len)
{
    debug_write_buffer(buff, len);
}

void uart3_write_buffer (const uint8 *buff, uint32 len)
{
    uart3_send_complete_flag = false;
    g_uart3.p_api->write(g_uart3.p_ctrl, (const uint8*)buff, len);     // 发送单个字符
    while(!uart3_send_complete_flag);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     串口发送字符串
// 参数说明     uart_n          串口模块号 参照 zf_driver_uart.h 中 uart_index_enum 枚举定义
// 参数说明     *str            要发送的字符串地址
// 返回参数     void
// 使用示例     uart_write_string("seekfree"); 
// 备注信息     
//-------------------------------------------------------------------------------------------------------------------
// UART9 发送字符串
void uart9_write_string (const char *str)
{
    while(*str)                                                                 // 一直循环到结尾
    {
        uart9_write_byte(*str ++);                                       // 发送数据
    }
}

void uart3_write_string (const char *str)
{
    while(*str)                                                                 // 一直循环到结尾
    {
        uart3_write_byte(*str ++);                                       // 发送数据
    }
}

//-------------------------------------------------------------------------------------------------------------------
// 函数简介     串口初始化
// 参数说明     uart_n          串口模块号 参照 zf_driver_uart.h 中 uart_index_enum 枚举定义
// 参数说明     baud            串口波特率
// 参数说明     tx_pin          串口发送引脚 参照 zf_driver_uart.h 中 uart_tx_pin_enum 枚举定义
// 参数说明     rx_pin          串口接收引脚 参照 zf_driver_uart.h 中 uart_rx_pin_enum 枚举定义
// 返回参数     void            
// 使用示例     uart_init(UART_1, 115200, UART1_TX_B12, UART1_RX_B13);          // 初始化串口1 波特率115200 发送引脚使用PA09 接收引脚使用PA10
// 备注信息     
//-------------------------------------------------------------------------------------------------------------------
// 初始化UART9 - 使用debug_init
void uart9_init()
{
    debug_init();  // 直接使用debug模块的初始化函数
}

void uart3_init()
{
    g_uart3.p_api->open(g_uart3.p_ctrl, g_uart3.p_cfg);      
}
