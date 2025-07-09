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
* �ļ�����          zf_device_scc8660
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
#include "zf_device_scc8660.h"
#include "zf_device_config.h"

volatile uint8  scc8660_finish_flag;
uint16  scc8660_buff[SCC8660_W*SCC8660_H] BSP_ALIGN_VARIABLE(64);// BSP_ALIGN_VARIABLE(64) BSP_PLACE_IN_SECTION(".sdram");

// ��Ҫ���õ�����ͷ������ �����������޸Ĳ���
static int16 scc8660_set_confing_buffer[SCC8660_CONFIG_FINISH][2]=
{
    {SCC8660_INIT,              0},                                             // ����ͷ��ʼ��ʼ��

    {SCC8660_AUTO_EXP,          SCC8660_AUTO_EXP_DEF},                          // �Զ��ع�
    {SCC8660_BRIGHT,            SCC8660_BRIGHT_DEF},                            // ��������
    {SCC8660_FPS,               SCC8660_FPS_DEF},                               // ͼ��֡��
    {SCC8660_SET_COL,           SCC8660_W},                                     // ͼ������
    {SCC8660_SET_ROW,           SCC8660_H},                                     // ͼ������
    {SCC8660_PCLK_DIV,          SCC8660_PCLK_DIV_DEF},                          // PCLK��Ƶϵ��
    {SCC8660_PCLK_MODE,         SCC8660_PCLK_MODE_DEF},                         // PCLKģʽ
    {SCC8660_COLOR_MODE,        SCC8660_COLOR_MODE_DEF},                        // ͼ��ɫ��ģʽ
    {SCC8660_DATA_FORMAT,       SCC8660_DATA_FORMAT_DEF},                       // ������ݸ�ʽ
    {SCC8660_MANUAL_WB,         SCC8660_MANUAL_WB_DEF}                          // �ֶ���ƽ��
};

void g_ceu0_user_callback (capture_callback_args_t* p_args)
{
    if((p_args->event & CEU_EVENT_FRAME_END) == CEU_EVENT_FRAME_END)
    {
        scc8660_finish_flag = 1;
    }
}

uint8 scc8660_init (void)
{
    system_delay_ms(100);
    soft_iic_info_struct scc8660_iic_struct;
    soft_iic_init(&scc8660_iic_struct, 0, SCC8660_COF_IIC_DELAY, SCC8660_COF_IIC_SCL, SCC8660_COF_IIC_SDA);
    if(scc8660_set_config_sccb(&scc8660_iic_struct, scc8660_set_confing_buffer))
    {
        return 1;
    }
    g_ceu0.p_api->open(g_ceu0.p_ctrl, g_ceu0.p_cfg);
    g_ceu0.p_api->captureStart(g_ceu0.p_ctrl, (uint8*)scc8660_buff);
	R_CEU->CAPCR = R_CEU_CAPCR_CTNCP_Msk;
    return 0;
}