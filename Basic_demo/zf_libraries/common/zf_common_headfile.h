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
* �ļ�����          zf_common_headfile
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
#ifndef _zf_common_headfile_h_
#define _zf_common_headfile_h_

//====================================================��׼��==========================================================
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "math.h"
//====================================================��׼��==========================================================

//===================================================�̹߳�����=======================================================
#include "hal_data.h"
//===================================================�̹߳�����=======================================================

//====================================================������==========================================================
#include "zf_common_fifo.h"
#include "zf_common_typedef.h"
#include "zf_common_function.h"
#include "zf_common_debug.h"
//====================================================������==========================================================


//===================================================����豸������===================================================
#include "zf_device_imu660ra.h"
#include "zf_device_wifi_uart.h"
#include "zf_device_ips200pro.h"
#include "zf_device_scc8660.h"
//===================================================����豸������===================================================

//===================================================оƬ����������===================================================
#include "ra_driver_sdram.h"
#include "zf_driver_gpio.h"
#include "zf_driver_delay.h"
#include "zf_driver_dac.h"
#include "zf_driver_pwm.h"
#include "zf_driver_adc.h"
#include "zf_driver_encoder.h"
#include "zf_driver_soft_iic.h"
#include "zf_driver_uart.h"
#include "zf_driver_sd.h"
#include "zf_driver_pit.h"
//===================================================оƬ����������===================================================


#endif
