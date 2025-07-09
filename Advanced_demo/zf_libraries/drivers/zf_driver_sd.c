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
* �ļ�����          zf_driver_sd
* ��˾����          �ɶ���ɿƼ����޹�˾
* �汾��Ϣ          �鿴 libraries/doc �ļ����� version �ļ� �汾˵��
* ��������          MDK 5.38
* ����ƽ̨          RA8D1
* ��������          https://seekfree.taobao.com/
* 
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
*                               ע
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "zf_driver_sd.h"

static uint8 g_transfer_complete = 0;
static uint8 g_card_erase_complete = 0;
static bool g_card_inserted = false;

uint8_t g_dest[SDHI_MAX_BLOCK_SIZE] BSP_ALIGN_VARIABLE(4);  //4�ֽڶ���
uint8_t g_src[SDHI_MAX_BLOCK_SIZE]  BSP_ALIGN_VARIABLE(4);
 
void g_sdmmc1_callback (sdmmc_callback_args_t * p_args)
{
     if (SDMMC_EVENT_TRANSFER_COMPLETE == p_args->event)  //��ȡ��д�����
     {
         g_transfer_complete = 1;
     }
     if (SDMMC_EVENT_CARD_INSERTED == p_args->event)  //�������ж�
     {
         g_card_inserted = true;
     }
     if (SDMMC_EVENT_CARD_REMOVED == p_args->event)   //���γ��ж�
     {
         g_card_inserted = false;
     }
     if (SDMMC_EVENT_ERASE_COMPLETE == p_args->event)  //�������
     {
         g_card_erase_complete = 1;
     }
     if (SDMMC_EVENT_ERASE_BUSY == p_args->event)  //������ʱ
     {
         g_card_erase_complete = 2;
     }
}


void sd_read_write(void)
{
    sdmmc_status_t status;
    sdmmc_device_t my_sdmmc_device = {0};
    uint32_t i;

    for (i = 0; i < SDHI_MAX_BLOCK_SIZE; i++)
    {
        g_src[i] = (uint8_t)('A' + (uint8_t)(i % 26));
    }

    g_sdmmc1.p_api->statusGet(g_sdmmc1.p_ctrl, &status);
    if (!status.card_inserted)
    {
        while (!g_card_inserted)
        {
            system_delay_ms(1000);
        }
    }

    system_delay_ms(10);
    g_sdmmc1.p_api->mediaInit(g_sdmmc1.p_ctrl, &my_sdmmc_device);

    /* д������ */
    g_transfer_complete = 0;
    g_sdmmc1.p_api->write(g_sdmmc1.p_ctrl, g_src, 1, 1);
    while (g_transfer_complete == 0);
    g_transfer_complete = 0;

    /* �������� */
    g_transfer_complete = 0;
    g_sdmmc1.p_api->read(g_sdmmc1.p_ctrl, g_dest, 1, 1);
    while (g_transfer_complete == 0);
    g_transfer_complete = 0;

    /* ��ӡ���� */
    for (i = 0; i < SDHI_MAX_BLOCK_SIZE; i++)
    {
        if (i % 26 == 0 && i > 0)
        {
            printf(" ");
        }
        printf("%c", g_dest[i]);
    }

    /* �Ա����� */
    if (strncmp((char*)&g_dest[0], (char*)&g_src[0], SDHI_MAX_BLOCK_SIZE) == 0)
    {
        printf("\r\nSD write read success!\r\n");
    }
    else
    {
        printf("\r\nSD write read fault!\r\n");
    }
}
 
void sd_init()
{
    g_sdmmc1.p_api->open(g_sdmmc1.p_ctrl, g_sdmmc1.p_cfg);
}
