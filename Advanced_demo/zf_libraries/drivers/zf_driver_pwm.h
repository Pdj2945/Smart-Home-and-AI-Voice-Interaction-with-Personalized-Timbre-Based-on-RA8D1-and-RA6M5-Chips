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
#ifndef _ZF_DRIVER_PWM_H_
#define _ZF_DRIVER_PWM_H_

#include "hal_data.h"
#include "zf_common_typedef.h"

#define MOTOR1_DIR              BSP_IO_PORT_04_PIN_14
#define MOTOR1_TIMER            &motor_timer0
#define MOTOR1_GTIOC            GPT_IO_PIN_GTIOCA
#define MOTOR2_DIR              BSP_IO_PORT_07_PIN_12
#define MOTOR2_TIMER            &motor_timer2
#define MOTOR2_GTIOC            GPT_IO_PIN_GTIOCA
#define MOTOR3_DIR              BSP_IO_PORT_09_PIN_11
#define MOTOR3_TIMER            &motor_timer3
#define MOTOR3_GTIOC            GPT_IO_PIN_GTIOCA
#define MOTOR4_DIR              BSP_IO_PORT_09_PIN_14
#define MOTOR4_TIMER            &motor_timer5
#define MOTOR4_GTIOC            GPT_IO_PIN_GTIOCA

#define SERVO1_TIMER            &servo_timer6
#define SERVO1_GTIOC            GPT_IO_PIN_GTIOCA
#define SERVO2_TIMER            &servo_timer6
#define SERVO2_GTIOC            GPT_IO_PIN_GTIOCB
#define SERVO3_TIMER            &servo_timer9
#define SERVO3_GTIOC            GPT_IO_PIN_GTIOCA
#define SERVO4_TIMER            &servo_timer9
#define SERVO4_GTIOC            GPT_IO_PIN_GTIOCB

void pwm_init   (const timer_instance_t *g_timer);
void pwm_set_duty   (const timer_instance_t *g_timer, gpt_io_pin_t pin_t, uint32_t duty);

#endif /* _ZF_DRIVER_PWM_H_ */
