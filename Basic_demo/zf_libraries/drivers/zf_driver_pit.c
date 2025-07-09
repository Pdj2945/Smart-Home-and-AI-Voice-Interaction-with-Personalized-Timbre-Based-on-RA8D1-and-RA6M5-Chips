/*********************************************************************************************************************
* RA8D1 Opensourec Library ����RA8D1 ��Դ�⣩��һ�����ڹٷ� SDK �ӿڵĵ�������Դ��
* Copyright (c) 2025 SEEKFREE ��ɿƼ�
* 
* ���ļ��� RA8D1 ��Դ���һ����
* 
* RA8D1 ��Դ�� ��������
* �����Ը��������������ᷢ���� GPL��GNU General Public License���� GNUͨ�ù������֤��������
* �� GPL �ĵ�3�棨�� GPL3.0������ѡ��ģ��κκ����İ汾�����·�����/���޸���
* 
* ����Դ��ķ�����ϣ�����ܷ������ã�����δ�������κεı�֤
* ����û�������������Ի��ʺ��ض���;�ı�֤
* ����ϸ����μ� GPL
* 
* ��Ӧ�����յ�����Դ���ͬʱ�յ�һ�� GPL �ĸ���
* ���û�У������<https://www.gnu.org/licenses/>
* 
* ����ע����
* ����Դ��ʹ�� GPL3.0 ��Դ���֤Э�� �����������Ϊ���İ汾
* �������Ӣ�İ��� libraries/doc �ļ����µ� GPL3_permission_statement.txt �ļ���
* ���֤������ libraries �ļ����� �����ļ����µ� LICENSE �ļ�
* ��ӭ��λʹ�ò����������� ���޸�����ʱ���뱣����ɿƼ��İ�Ȩ����������������
* 
* �ļ�����          zf_driver_pit
* ��˾����          �ɶ���ɿƼ����޹�˾
* �汾��Ϣ          �鿴 libraries/doc �ļ����� version �ļ� �汾˵��
* ��������          MDK 5.38
* ����ƽ̨          RA8D1
* ��������          https://seekfree.taobao.com/
* 
* �޸ļ�¼
* ����              ����                ��ע
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
    g_timer0.p_api->open(g_timer0.p_ctrl, g_timer0.p_cfg);  // ����5ms��ʱ��
    g_timer0.p_api->start(g_timer0.p_ctrl);  
    
    g_timer1.p_api->open(g_timer1.p_ctrl, g_timer1.p_cfg);  // ����125us��ʱ��
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