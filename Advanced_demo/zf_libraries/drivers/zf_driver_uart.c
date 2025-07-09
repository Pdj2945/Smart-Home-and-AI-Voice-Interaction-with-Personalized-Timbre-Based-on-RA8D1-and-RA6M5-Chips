/*********************************************************************************************************************
* RA8D1 Opensourec Library （RA8D1 源码库）是一个基于RA8D1 芯片的 SDK 接口的源码库
* Copyright (c) 2025 SEEKFREE 千如科技
* 
* 本文件是 RA8D1 源码库的一部分
* 
* RA8D1 源码库 包含以下内容
* 本代码遵循GPL开源协议，如有违反，将受到GPL（GNU General Public License，GNU通用公共许可证）的约束。
* 在GPL的第3版（即GPL3.0）下，您可以选择，任何后续的版本，进行分发/修改。
* 本代码的分发，希望您能遵守，但未提供任何保证。
* 即没有对适销性以及特定用途的保证。
* 详情请见GPL。
* 如果您有疑问，请访问<https://www.gnu.org/licenses/>。
* 本代码的英文版本在libraries/doc文件夹下的GPL3_permission_statement.txt文件中。
* 本许可证在libraries文件夹下，即本文件夹下的LICENSE文件。
* 欢迎各位使用本代码，但修改时请保留千如科技的版权，谢谢合作。
* 
* 修改记录
* 日期              修改人          备注
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "zf_driver_uart.h"
#define UART3_RX_BUF_SIZE 256
extern volatile uint8_t uart3_rx_buf[UART3_RX_BUF_SIZE];  // UART3接收缓冲区
extern volatile uint16_t uart3_rx_head; // 接收缓冲区头指针
extern volatile uint16_t uart3_rx_tail; // 接收缓冲区尾指针
extern uint8_t tts_response_buffer[256]; // TTS回复缓冲区
extern uint16_t tts_response_len;    // TTS回复长度
extern uint8_t tts_response_ready;   // TTS回复就绪标志
static volatile bool uart_send_complete_flag = false;
void uart3_callback (uart_callback_args_t* p_args)
{
    switch(p_args->event)
    {
    case UART_EVENT_RX_CHAR:
    {
        //g_uart3.p_api->write(g_uart3.p_ctrl, (const uint8*)&(p_args->data), 1); 
        //break;
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
        uart_send_complete_flag = true;
        break;
    }
    default:
        break;
    }
}
//-------------------------------------------------------------------------------------------------------------------
// 函数说明     uart_n          串口模块号，参考zf_driver_uart.h中的uart_index_enum枚举定义
// 函数说明     dat             要发送的字节
// 返回值       void        
// 示例         uart_write_byte(0xA5);                                  // 发送一个字节0xA5
// 备注       
//-------------------------------------------------------------------------------------------------------------------
void uart3_write_byte (const uint8 dat)
{
    uart_send_complete_flag = false;
    g_uart3.p_api->write(g_uart3.p_ctrl, (const uint8*)&dat, 1);     // 发送字节
    while(!uart_send_complete_flag);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     uart_n          串口模块号，参考zf_driver_uart.h中的uart_index_enum枚举定义
// 函数说明     *buff          要发送的缓冲区
// 函数说明     len             缓冲区长度
// 返回值       void
// 示例         uart_write_buffer(&a[0], 5);
// 备注       
//-------------------------------------------------------------------------------------------------------------------
void uart3_write_buffer (const uint8 *buff, uint32 len)
{
    uart_send_complete_flag = false;
    g_uart3.p_api->write(g_uart3.p_ctrl, (const uint8*)buff, len);     // 发送缓冲区
    while(!uart_send_complete_flag);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     uart_n          串口模块号，参考zf_driver_uart.h中的uart_index_enum枚举定义
// 函数说明     *str           要发送的字符串
// 返回值       void
// 示例         uart_write_string("seekfree"); 
// 备注       
//-------------------------------------------------------------------------------------------------------------------
void uart3_write_string (const char *str)
{
    while(*str)                                                                 // 一直循环到字符串结束
    {
        uart3_write_byte(*str ++);                                       // 发送字符
    }
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     uart_n          串口模块号，参考zf_driver_uart.h中的uart_index_enum枚举定义
// 函数说明     baud            波特率
// 函数说明     tx_pin          用于发送引脚，参考zf_driver_uart.h中的uart_tx_pin_enum枚举定义
// 函数说明     rx_pin          用于接收引脚，参考zf_driver_uart.h中的uart_rx_pin_enum枚举定义
// 返回值       void            
// 示例         uart_init(UART_1, 115200, UART1_TX_B12, UART1_RX_B13);          // 初始化串口1 波特率115200 使用PA09 作为发送引脚，使用PA10作为接收引脚
// 备注       
//-------------------------------------------------------------------------------------------------------------------
void uart3_init()
{
    g_uart3.p_api->open(g_uart3.p_ctrl, g_uart3.p_cfg);      
}
