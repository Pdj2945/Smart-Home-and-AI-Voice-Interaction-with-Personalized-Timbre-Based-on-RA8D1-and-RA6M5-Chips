/**
 * @file bsp_uart_debug.c
 * @brief UART debug interface implementation
 * @details This file implements UART debug functionality for printf redirection
 */

 #include "hal_data.h"
 #include "bsp_uart.h"
 #include "stdio.h"
 
 /* Flag to indicate UART transmission completion */
volatile bool uart_send_complete_flag = false;
uint8_t g_uart6_rx_buf[UART6_RX_BUF_SIZE];
volatile uint8_t g_uart6_rx_flag;
 /**
  * @brief UART callback function for handling UART events
  * @param p_args Pointer to UART callback arguments
  */
 void uart6_callback(uart_callback_args_t *p_args)
 {
     if (p_args->event == UART_EVENT_TX_COMPLETE)
     {
         uart_send_complete_flag = true;
     }
 }
 void bsp_uart6_start_receive(void)
{
    g_uart6_rx_flag = 0;
    g_uart6.p_api->read(g_uart6.p_ctrl, g_uart6_rx_buf, UART6_RX_BUF_SIZE);
}
 
 /**
  * @brief Initialize UART debug interface
  * @return void
  */
 void uart_init(void)
 {
     fsp_err_t err;
     err = R_SCI_UART_Open(g_uart6.p_ctrl, g_uart6.p_cfg);
     assert(FSP_SUCCESS == err);
 }
 
 #if defined __GNUC__ && !defined __clang__
 /**
  * @brief Write function for GCC compiler
  * @param fd File descriptor (unused)
  * @param pBuffer Buffer containing data to write
  * @param size Number of bytes to write
  * @return Number of bytes written
  */
 int _write(int fd, char *pBuffer, int size)
 {
     (void)fd;
     R_SCI_UART_Write(&g_uart6_ctrl, (uint8_t *)pBuffer, (uint32_t)size);
     while (uart_send_complete_flag == false);
     uart_send_complete_flag = false;
 
     return size;
 }
 #else
 /**
  * @brief Write function for other compilers
  * @param ch Character to write
  * @param f File pointer (unused)
  * @return Character written
  */
 int fputc(int ch, FILE *f)
 {
     (void)f;
     R_SCI_UART_Write(&g_uart6_ctrl, (uint8_t *)&ch, 1);
     while (uart_send_complete_flag == false);
     uart_send_complete_flag = false;
 
     return ch;
 }
 #endif
 
