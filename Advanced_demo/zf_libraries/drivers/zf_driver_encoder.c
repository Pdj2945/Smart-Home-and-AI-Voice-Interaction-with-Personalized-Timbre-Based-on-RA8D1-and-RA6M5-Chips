/*********************************************************************************************************************
* RA8D1 Opensourec Library ����RA8D1 ��Դ�⣩��һ�����ڹٷ� SDK �ӿڵĵ�������Դ��
* Copyright (c) 2025 SEEKFREE ��ɿƼ�
* 
* ���ļ��� RA8D1 ��Դ���һ����
* 
* RA8D1 ��Դ�� ���������
* �����Ը���������������ᷢ���� GPL��GNU General Public License���� GNUͨ�ù�������֤��������
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
* ����Դ��ʹ�� GPL3.0 ��Դ����֤Э�� ������������Ϊ���İ汾
* ��������Ӣ�İ��� libraries/doc �ļ����µ� GPL3_permission_statement.txt �ļ���
* ����֤������ libraries �ļ����� �����ļ����µ� LICENSE �ļ�
* ��ӭ��λʹ�ò����������� ���޸�����ʱ���뱣����ɿƼ��İ�Ȩ����������������
* 
* �ļ�����          zf_driver_encoder
* ��˾����          �ɶ���ɿƼ����޹�˾
* �汾��Ϣ          �鿴 libraries/doc �ļ����� version �ļ� �汾˵��
* ��������          MDK 5.38
* ����ƽ̨          RA8D1
* ��������          https://seekfree.taobao.com/
* 
* 修改记录
*                               ע
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "zf_driver_encoder.h"

//----------------------------------------------------------------------------------------
// �������     ��ʼ����ʱ��
// ����˵��     g_timer        ������timer����
// ���ز���     void
// ʹ��ʾ��     encoder_init(g_timer1);
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
void encoder_init (const timer_instance_t *g_timer)
{
    g_timer->p_api->open(g_timer->p_ctrl, g_timer->p_cfg);
    g_timer->p_api->start(g_timer->p_ctrl);
    //初始化完成后，第一次读取值
    g_timer->p_api->reset(g_timer->p_ctrl);
}

//----------------------------------------------------------------------------------------
// �������     ��ȡ��ʱ��ֵ
// ����˵��     g_timer        ������timer����
// ����˵��     dir_pin         ��������������
// ���ز���     int32_t         ���ؼ�����ֵ
// ʹ��ʾ��     encoder_get_count(g_timer1, pin1);
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
int32_t encoder_get_count (const timer_instance_t *g_timer, bsp_io_port_pin_t dir_pin)
{
    bsp_io_level_t _level;
    timer_status_t now_count = {0};

    //��ȡ��ǰ����ֵ
    g_timer->p_api->statusGet(g_timer->p_ctrl, &now_count);

    //��ȡ�������ŵ�ƽ
    g_ioport.p_api->pinRead(g_ioport.p_ctrl, dir_pin, &_level);

    //�жϷ������ŵ�ƽ���ض�Ӧ����ֵ
    if(_level)
    {
        return (int32_t)now_count.counter;
    }
    else
    {
        return -(int32_t)now_count.counter;
    }
}

//----------------------------------------------------------------------------------------
// �������     ��ռ�ʱ��
// ����˵��     g_timer        ������timer����
// ���ز���     void
// ʹ��ʾ��     encoder_clear_count(g_timer1);
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
void encoder_clear_count (const timer_instance_t *g_timer)
{
    g_timer->p_api->reset(g_timer->p_ctrl);
}
