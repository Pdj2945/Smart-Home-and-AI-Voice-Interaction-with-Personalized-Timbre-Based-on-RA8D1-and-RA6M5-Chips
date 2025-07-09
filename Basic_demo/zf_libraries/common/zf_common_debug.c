/*********************************************************************************************************************
* RA8D1 Opensourec Library ����RA8D1 ��Դ�⣩��һ�����ڹٷ� SDK �ӿڵĵ�������Դ��
* Copyright (c) 2025 SEEKFREE ��ɿƼ�?
* 
* ���ļ��� RA8D1 ��Դ���һ����?
* 
* RA8D1 ��Դ�� ���������?
* �����Ը����������������?���� GPL��GNU General Public License���� GNUͨ�ù�������֤��������
* �� GPL �ĵ�3�棨�� GPL3.0������ѡ��ģ��κκ����İ汾�����·�����?/���޸���
* 
* ����Դ��ķ�����ϣ�����ܷ������ã�����δ�������κεı��?
* ����û���������Ի��ʺ��ض���;�ı�֤
* ����ϸ����μ�? GPL
* 
* ��Ӧ�����յ�����Դ����?ʱ�յ�һ�� GPL �ĸ���
* ���û�У������<https://www.gnu.org/licenses/>
* 
* ����ע����
* ����Դ��ʹ�� GPL3.0 ��Դ����֤Э�� ������������Ϊ���İ汾
* ��������Ӣ�İ��� libraries/doc �ļ����µ� GPL3_permission_statement.txt �ļ���
* ����֤������ libraries �ļ����� �����ļ����µ� LICENSE �ļ�
* ��ӭ��λʹ�ò����������� ���޸�����ʱ���뱣����ɿƼ��İ��?����������������
* 
* �ļ�����          zf_common_debug
* ��˾����          �ɶ���ɿƼ����޹��?
* �汾��Ϣ          �鿴 libraries/doc �ļ����� version �ļ� �汾˵��
* ��������          MDK 5.38
* ����ƽ̨          RA8D1
* ��������          https://seekfree.taobao.com/
* 
* �޸ļ�¼
* ����              ����                ��ע
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "zf_common_debug.h"

static volatile bool uart_send_complete_flag = false;
volatile uint8_t uart9_rx_buf[UART9_RX_BUF_SIZE];
volatile uint16_t uart9_rx_head = 0;
volatile uint16_t uart9_rx_tail = 0;

void uart9_callback (uart_callback_args_t* p_args)
{
    switch(p_args->event)
    {
    case UART_EVENT_RX_CHAR:
        {
			//g_uart9.p_api->write(g_uart9.p_ctrl, (const uint8*)&(p_args->data), 1); 
            uint16_t next_head = (uart9_rx_head + 1) % UART9_RX_BUF_SIZE;
            if (next_head != uart9_rx_tail) // ������δ��
            {
                uart9_rx_buf[uart9_rx_head] = (uint8_t)p_args->data;
                uart9_rx_head = next_head;
            }
            // ���������ݣ��ɼ����������
        }
        break;
    case UART_EVENT_TX_COMPLETE:
        uart_send_complete_flag = true;
        break;
    default:
        break;
    }
}


int32_t fputc (int32_t ch, FILE* f)
{
    (void)f;
    uart_send_complete_flag = false;
    g_uart9.p_api->write(g_uart9.p_ctrl, (const uint8*)&ch, 1);     // ���͵�����
    while(!uart_send_complete_flag);
    return ch;
}

void debug_write_buffer (const uint8 *buff, uint32 len)
{
    uart_send_complete_flag = false;
    g_uart9.p_api->write(g_uart9.p_ctrl, (const uint8*)buff, len);     // ���͵�����
    while(!uart_send_complete_flag);
}


//-------------------------------------------------------------------------------------------------------------------
// �������?     debug ���ڳ�ʼ��
// ����˵��     void
// ���ز���     void
// ʹ��ʾ��     debug_init();
// ��ע��Ϣ     ��Դ��ʾ��Ĭ�ϵ��� ��Ĭ�Ͻ����жϽ���
//-------------------------------------------------------------------------------------------------------------------
void debug_init (void)
{
    g_uart9.p_api->open(g_uart9.p_ctrl, g_uart9.p_cfg);      
}

