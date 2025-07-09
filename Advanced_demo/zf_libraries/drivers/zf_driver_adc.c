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
* �ļ�����          zf_driver_adc
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
#include "zf_driver_adc.h"

volatile bool adc_complete_flag = false;

void adc_callback (adc_callback_args_t* p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
    adc_complete_flag = true;
    g_adc0.p_api->scanStop(g_adc0.p_ctrl);
}

//----------------------------------------------------------------------------------------
// �������     ��ȡADC��ֵ
// ����˵��     void
// ���ز���     void
// ʹ��ʾ��     adc_get_voltage();
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
double adc_get_voltage (void)
{
    uint16_t adc_data = 0;
    double voltage_out;
    g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    while(!adc_complete_flag);                                      // �ȴ�ת����ɱ�־
    adc_complete_flag = false;                                      // ���������־λ
                                                                       
    g_adc0.p_api->read(g_adc0.p_ctrl, ADC_CHANNEL_0, &adc_data);    // ��ȡͨ��0����
    voltage_out = (double)(adc_data * 3.3 / 4095) * 11;             // ADCԭʼ����ת��Ϊ��ѹֵ��ADC�ο���ѹΪ3.3V��
    return voltage_out;
}

//----------------------------------------------------------------------------------------
// �������     ��ȡADC��ֵ
// ����˵��     void
// ���ز���     void
// ʹ��ʾ��     adc_get_voltage();
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
uint16_t adc_get_mic (void)
{
    uint16_t adc_data = 0;

    g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    while(!adc_complete_flag);                                      // �ȴ�ת����ɱ�־
    adc_complete_flag = false;                                      // ���������־λ
                                                                       
    g_adc0.p_api->read(g_adc0.p_ctrl, ADC_CHANNEL_5, &adc_data);    // ��ȡͨ��0����
    return adc_data;
}

//----------------------------------------------------------------------------------------
// �������     ��ȡADC��ֵ
// ����˵��     void
// ���ز���     void
// ʹ��ʾ��     adc_get_random();
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
uint16_t adc_get_random (void)
{
    uint16_t adc_data = 0;

    g_adc0.p_api->scanStart(g_adc0.p_ctrl);
    while(!adc_complete_flag);                                      // �ȴ�ת����ɱ�־
    adc_complete_flag = false;                                      // ���������־λ
                                                                       
    g_adc0.p_api->read(g_adc0.p_ctrl, ADC_CHANNEL_6, &adc_data);    // ��ȡͨ��0����
    return adc_data;
}

//----------------------------------------------------------------------------------------
// �������     ADC��ʼ��
// ����˵��     void
// ���ز���     void
// ʹ��ʾ��     adc_init();
// ��ע��Ϣ     
//----------------------------------------------------------------------------------------
void adc_init (void)
{
    g_adc0.p_api->open(g_adc0.p_ctrl, g_adc0.p_cfg);
    g_adc0.p_api->scanCfg(g_adc0.p_ctrl, g_adc0.p_channel_cfg);
}

