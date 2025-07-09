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
* �ļ�����          zf_driver_pwm
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
#include "zf_driver_pwm.h"
//----------------------------------------------------------------------------------------
// �������     ��ʼ�� PWM
// ����˵��     g_timer        ������Ŷ�Ӧ��timer����
// ���ز���     void
// ʹ��ʾ��     pwm_init(g_timer1);
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
void pwm_init (const timer_instance_t *g_timer)
{
    g_timer->p_api->open(g_timer->p_ctrl, g_timer->p_cfg);
    g_timer->p_api->start(g_timer->p_ctrl);
}

//----------------------------------------------------------------------------------------
// �������     ����PWMռ�ձ�
// ����˵��     g_timer        ������Ŷ�Ӧ��timer����
// ����˵��     pin_t           ѡ��timerͨ��A��B
// ����˵��     duty           PWMռ�ձȣ�1~10000��
// ���ز���     void
// ʹ��ʾ��     pwm_duty(g_timer1, GPT_IO_PIN_GTIOCA, 2000);
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
void pwm_set_duty (const timer_instance_t *g_timer, gpt_io_pin_t pin_t, uint32_t duty)
{
    timer_info_t info;
    uint32_t current_period_counts;
    uint32_t duty_cycle_counts;

    if (10000 < duty) duty = 10000;

    R_GPT_InfoGet(g_timer->p_ctrl, &info);

    current_period_counts = info.period_counts;
    duty_cycle_counts = (uint32_t)(((uint64_t) current_period_counts * duty) / 10000);

    R_GPT_DutyCycleSet(g_timer->p_ctrl, duty_cycle_counts, pin_t);
}
