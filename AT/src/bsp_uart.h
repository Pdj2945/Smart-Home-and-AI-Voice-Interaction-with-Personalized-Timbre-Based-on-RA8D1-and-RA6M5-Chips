
 #ifndef BSP_UART_H_
 #define BSP_UART_H_

 
 #include "hal_data.h"
 #define UART6_RX_BUF_SIZE  64

 extern uint8_t g_uart6_rx_buf[UART6_RX_BUF_SIZE];
 extern volatile uint8_t g_uart6_rx_flag;

 void bsp_uart6_start_receive(void);
 void uart_init(void);


 
 #endif /* BSP_UART_H_ */
 
