/*********************************************************************************************************************
* RA8D1 Opensourec Library 即（RA8D1 开源库）是一个基于官方 SDK 接口的第三方开源库
* Copyright (c) 2025 SEEKFREE 逐飞科技
* 
* 本文件是 RA8D1 开源库的一部分
* 
* RA8D1 开源库 是免费软件
* 您可以根据自由软件基金会发布的 GPL（GNU General Public License，即 GNU通用公共许可证）的条款
* 即 GPL 的第3版（即 GPL3.0）或（您选择的）任何后来的版本，重新发布和/或修改它
* 
* 本开源库的发布是希望它能发挥作用，但并未对其作任何的保证
* 甚至没有隐含的适销性或适合特定用途的保证
* 更多细节请参见 GPL
* 
* 您应该在收到本开源库的同时收到一份 GPL 的副本
* 如果没有，请参阅<https://www.gnu.org/licenses/>
* 
* 额外注明：
* 本开源库使用 GPL3.0 开源许可证协议 以上许可申明为译文版本
* 许可申明英文版在 libraries/doc 文件夹下的 GPL3_permission_statement.txt 文件中
* 许可证副本在 libraries 文件夹下 即该文件夹下的 LICENSE 文件
* 欢迎各位使用并传播本程序 但修改内容时必须保留逐飞科技的版权声明（即本声明）
* 
* 文件名称          hal_entry
* 公司名称          成都逐飞科技有限公司
* 版本信息          查看 libraries/doc 文件夹内 version 文件 版本说明
* 开发环境          MDK 5.38
* 适用平台          RA8D1
* 店铺链接          https://seekfree.taobao.com/
* 
* 修改记录
* 日期              作者                备注
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "hal_data.h"
#include "zf_common_headfile.h"
#include "tcp_uart.h"
// 注释掉TTS功能头文件
// #include "tts_functions.h"

// 前向声明 - 电机控制相关函数
bool motor_execute(uint8_t motor_id, uint8_t control_type, uint16_t repeat_count, uint32_t period_time);

// 方向定义
typedef enum {
    DIR_NORTH = 0,  // 北方（上）
    DIR_EAST = 1,   // 东方（右）
    DIR_SOUTH = 2,  // 南方（下）
    DIR_WEST = 3    // 西方（左）
} Direction;

// 方向描述字符串
const char* direction_names[] = {
    "北", "东", "南", "西"
};

// 小车状态结构体
typedef struct {
    int x;           // x 坐标
    int y;           // y 坐标
    Direction dir;   // 朝向
} CarState;

// 小车状态全局变量
CarState car_state = {0, 0, DIR_NORTH};
// 方向全局变量，供外部函数使用
Direction car_state_dir = DIR_NORTH;

// 特定基站数据结构
typedef struct {
    bool data_ready;                      // 数据是否就绪
    uint32_t distance;                    // 距离(cm)
    int16_t azimuth;                      // 方位角(度)
    int16_t elevation;                    // 仰角(度)
    float x_cm;                           // 原始X坐标(cm)
    float y_cm;                           // 原始Y坐标(cm)
    int16_t grid_x;                       // 网格X坐标
    int16_t grid_y;                       // 网格Y坐标
    int16_t filtered_x;                   // 滤波后X坐标
    int16_t filtered_y;                   // 滤波后Y坐标
    uint32_t last_update_time;            // 最后更新时间
} AnchorData;

// 前向声明基本控制函数
void turn_left(void);
void turn_right(void);
void turn_back(void);
void move_forward(void);
void move_forward_steps(int steps);
void report_position_to_cloud(void);

// ================== TCP通信相关定义 ==================
// 注释掉这里的WiFi配置，统一使用tcp_uart.h中定义的值
// #define WIFI_SSID           "ckf"          // WiFi名称
// #define WIFI_PASSWORD       "123456789"          // WiFi密码

// ================== 命令常量定义 ==================
// 将命令常量定义为全局变量，供其他文件使用
const char* CMD_TURN_LEFT = "left";          // 左转指令
const char* CMD_TURN_RIGHT = "right";         // 右转指令
const char* CMD_TURN_BACK = "back";          // 掉头指令
const char* CMD_FORWARD = "forward";       // 前进一步指令
const char* CMD_MOVE_STEPS = "move";          // 前进多步指令前缀
#define POSITION_INTERVAL    5000            // 位置上报间隔(ms)

// ================== 步进电机相关定义 ==================
// 第一个电机
#define STEPPER1_STP             BSP_IO_PORT_09_PIN_15    // 脉冲输出引脚 (MOTOR4_TIMER)
#define STEPPER1_DIR             BSP_IO_PORT_09_PIN_12    // 方向控制引脚
#define STEPPER1_EN              BSP_IO_PORT_09_PIN_11    // 使能控制引脚

// 第二个电机
#define STEPPER2_STP             BSP_IO_PORT_07_PIN_13    // 脉冲输出引脚 (MOTOR2_TIMER)
#define STEPPER2_DIR             BSP_IO_PORT_07_PIN_12    // 方向控制引脚
// 第二个电机不需要EN引脚，已设置为永终驱动

// 电机选择定义
#define MOTOR_SELECT_1           1                        // 控制第一个电机
#define MOTOR_SELECT_2           2                        // 控制第二个电机
#define MOTOR_SELECT_BOTH        3                        // 同时控制两个电机，方向相同
#define MOTOR_SELECT_OPPOSITE    4                        // 同时控制两个电机，方向相反

// 步进电机控制宏定义
#define STEPPER_ENABLE           0                       // 步进电机使能（低电平有效）
#define STEPPER_DISABLE          1                       // 步进电机禁用
#define STEPPER_DIR_CW           1                       // 顺时针方向
#define STEPPER_DIR_CCW          0                       // 逆时针方向

// 控制类型定义
#define CONTROL_ROTATION         0                       // 旋转控制模式
#define CONTROL_ANGLE            1                       // 角度控制模式

// CR_vFCC模式相关设置
#define STEPPER_MODE_CR_VFCC     1                       // CR_vFCC模式（闭环速度控制）

// 步进电机速度控制
#define TARGET_SPEED_HZ          200                       // 目标速度值(Hz)
#define STEPPER_PWM_DUTY         5000                        // PWM占空比（0-10000，这里设为50%）

// 电机单步脉冲数定义
#define PULSE_PER_DEGREE         1.92                        // 每度对应的脉冲数（经校准）

// 全局变量 - TCP通信相关
// 将tcp_connected改为外部引用
extern uint8_t tcp_connected;                // 连接状态
static uint32_t last_heartbeat_time = 0;  // 上次心跳时间
static uint32_t last_print_time = 0;      // 上次状态打印时间
static uint8_t receive_buffer[1024];      // 接收缓冲区
static char message_buffer[1024];         // 消息缓冲区
static uint8_t new_message_flag = 0;      // 新消息标志

// 全局变量 - 电机控制相关
uint8_t stepper1_dir = STEPPER_DIR_CW;    // 第一个电机方向
uint8_t stepper2_dir = STEPPER_DIR_CW;    // 第二个电机方向
uint32_t stepper_speed = TARGET_SPEED_HZ; // 电机速度(脉冲频率Hz)
uint8_t motor1_state = 1;                 // 第一个电机启停状态：1-运行，0-停止
uint8_t motor2_state = 1;                 // 第二个电机启停状态：1-运行，0-停止
uint8_t car_control_busy = 0;             // 车辆控制状态：0-空闲，1-执行中，2-刚执行完
uint32_t pulse_count = 0;                 // 脉冲计数器
uint32_t pulse_period = 0;                // 脉冲周期(ms)
float angle_correction = 1.0;             // 角度校准系数
float angle_correction2 = 1.0;            // 第二个电机的角度校准系数
uint32_t last_position_report_time = 0;   // 上次位置上报时间
uint8_t position_update_counter = 0;    // UWB位置更新计数器
bool waiting_for_position_update = false; // 等待位置更新标志

// 串口相关
extern volatile uint8_t uart9_rx_buf[];
extern volatile uint16_t uart9_rx_head, uart9_rx_tail;
extern volatile uint8_t uart3_rx_buf[];
extern volatile uint16_t uart3_rx_head, uart3_rx_tail;

// ================== 全局变量 ==================
uint32 system_time = 0;            // 系统时间计数器，去掉static使其全局可见
uint8 uart9_frame_buf[UART_FRAME_BUF_SIZE]; // 串口帧缓冲区
uint16 uart9_frame_len = 0;               // 串口帧长度
bool frame_in_progress = false;           // 是否在拼接一帧
char tcp_message_buffer[1024];            // TCP消息缓冲区

// UWB基站跟踪相关
#define MAX_ANCHOR_COUNT 8                // 最大支持的基站数量
uint32_t anchor_ids[MAX_ANCHOR_COUNT];    // 基站ID数组
uint8_t anchor_count = 0;                 // 已发现的基站数量
uint32_t last_anchor_id = 0;              // 上一个基站ID
bool is_multi_anchor = false;             // 是否存在多个基站

// 特定基站处理相关
#define ANCHOR_ID_1 0x177                 // 第一个基站ID
#define ANCHOR_ID_2 0x178                 // 第二个基站ID

AnchorData anchor1_data = {0};            // 基站1数据
AnchorData anchor2_data = {0};            // 基站2数据
bool dual_anchor_print_ready = false;     // 双基站打印就绪标志
uint32_t last_dual_print_time = 0;        // 上次双基站打印时间

// ================== 系统定时器相关 ==================
extern uint32_t SystemCoreClock;

void agt0_callback(timer_callback_args_t *p_args)
{
    if (TIMER_EVENT_CYCLE_END == p_args->event)
    {
        system_time++; // 每次定时器中断增加计数
    }
}

uint32 get_system_time(void)
{
    return system_time;
}

// ================== 函数声明 ==================
void stepper_cr_vfcc_init(void);
void stepper_generate_pulse(uint8_t motor_id);
void set_motor_direction(uint8_t motor_id, uint8_t direction);
void motor_run_time(uint8_t motor_id, uint32_t run_time_ms);
bool motor_rotate_pulses(uint8_t motor_id, uint16_t pulses);
void set_angle_correction(uint8_t motor_id, float target_angle, float actual_angle);
void motor_control(const char cmd);
void process_motor_command(const char* command);
// 必须先声明AnchorData结构体，再声明使用它的函数
void filter_position(int16 new_x, int16 new_y);
void filter_anchor_position(AnchorData *anchor_data);  // 添加新函数的前向声明

// ================== 步进电机控制函数实现 ==================

// CR_vFCC模式初始化函数
void stepper_cr_vfcc_init(void)
{
    printf("\r\n开始初始化步进电机...");
    
    // ----- 第一个电机初始化 -----
    // 设置引脚状态
    gpio_set_level(STEPPER1_EN, STEPPER_DISABLE);    // 先禁用电机
    system_delay_ms(100);                           // 等待稳定
    
    // 设置方向引脚
    gpio_set_level(STEPPER1_DIR, stepper1_dir);
    printf("\r\n电机1方向设置完成: %d", stepper1_dir);
    system_delay_ms(10);
    
    // 初始化PWM，用于生成脉冲信号
    pwm_init(MOTOR4_TIMER);  
    pwm_set_duty(MOTOR4_TIMER, MOTOR4_GTIOC, 0);  // 初始不输出脉冲
    printf("\r\n电机1 PWM初始化完成");
    
    // 使能电机驱动
    system_delay_ms(100);
    gpio_set_level(STEPPER1_EN, STEPPER_ENABLE);     // 使能电机
    printf("\r\n电机1使能完成");
    
    // ----- 第二个电机初始化 -----
    // 设置方向引脚
    gpio_set_level(STEPPER2_DIR, stepper2_dir);
    printf("\r\n电机2方向设置完成: %d", stepper2_dir);
    system_delay_ms(10);
    
    // 初始化PWM，用于生成脉冲信号
    pwm_init(MOTOR2_TIMER);  
    pwm_set_duty(MOTOR2_TIMER, MOTOR2_GTIOC, 0);  // 初始不输出脉冲
    printf("\r\n电机2 PWM初始化完成");
    
    // 计算初始脉冲周期
    pulse_period = 1000 / stepper_speed;
    
    system_delay_ms(100);                           // 等待驱动板完成初始化
    printf("\r\n步进电机初始化完成: 速度=%dHz, 周期=%dms", stepper_speed, pulse_period);
}

// 生成步进电机脉冲
void stepper_generate_pulse(uint8_t motor_id)
{
    pulse_count++;//脉冲计数
    
    // 根据速度生成脉冲
    if (pulse_count >= pulse_period)
    {
        // 根据电机ID生成对应的脉冲
        if(motor_id == MOTOR_SELECT_1 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
        {
            // 生成电机1的脉冲
            pwm_set_duty(MOTOR4_TIMER, MOTOR4_GTIOC, STEPPER_PWM_DUTY);
            system_delay_ms(1);  // 脉冲持续时间
            pwm_set_duty(MOTOR4_TIMER, MOTOR4_GTIOC, 0);
        }
        
        if(motor_id == MOTOR_SELECT_2 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
        {
            // 生成电机2的脉冲
            pwm_set_duty(MOTOR2_TIMER, MOTOR2_GTIOC, STEPPER_PWM_DUTY);
            system_delay_ms(1);  // 脉冲持续时间
            pwm_set_duty(MOTOR2_TIMER, MOTOR2_GTIOC, 0);
        }
        
        pulse_count = 0;  // 重置计数器
    }
}

// 设置电机方向
void set_motor_direction(uint8_t motor_id, uint8_t direction)
{
    // 如果是相反方向模式，电机2的方向与参数相反
    uint8_t dir2 = (motor_id == MOTOR_SELECT_OPPOSITE) ? !direction : direction;
    
    if(motor_id == MOTOR_SELECT_1 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
    {
        stepper1_dir = direction;
        gpio_set_level(STEPPER1_DIR, stepper1_dir);
    }
    
    if(motor_id == MOTOR_SELECT_2 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
    {
        stepper2_dir = dir2;
        gpio_set_level(STEPPER2_DIR, stepper2_dir);
    }
}

// 控制电机运行指定时间
void motor_run_time(uint8_t motor_id, uint32_t run_time_ms)
{
    printf("\r\n开始电机运行...");
    uint32_t start = get_system_time();
    uint32_t pulse_interval = 1000000 / stepper_speed; // 微秒级间隔
    
    while(get_system_time() - start < run_time_ms)
    {
        // 电机1控制
        if((motor_id == MOTOR_SELECT_1 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE) && motor1_state)
        {
            // 输出电机1脉冲
            pwm_set_duty(MOTOR4_TIMER, MOTOR4_GTIOC, STEPPER_PWM_DUTY);
            system_delay_us(50);  // 脉冲宽度
            pwm_set_duty(MOTOR4_TIMER, MOTOR4_GTIOC, 0);
        }
        
        // 电机2控制
        if((motor_id == MOTOR_SELECT_2 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE) && motor2_state)
        {
            // 输出电机2脉冲
            pwm_set_duty(MOTOR2_TIMER, MOTOR2_GTIOC, STEPPER_PWM_DUTY);
            system_delay_us(50);  // 脉冲宽度
            pwm_set_duty(MOTOR2_TIMER, MOTOR2_GTIOC, 0);
        }
        
        // 等待下一个脉冲
        system_delay_us(pulse_interval - 50);
    }
    
    // 停止所有脉冲输出
    if(motor_id == MOTOR_SELECT_1 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
    {
        pwm_set_duty(MOTOR4_TIMER, MOTOR4_GTIOC, 0);
    }
    
    if(motor_id == MOTOR_SELECT_2 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
    {
        pwm_set_duty(MOTOR2_TIMER, MOTOR2_GTIOC, 0);
    }
    
    printf("\r\n电机运行完成，时长: %dms", run_time_ms);
}

// 控制电机旋转指定脉冲数
bool motor_rotate_pulses(uint8_t motor_id, uint16_t pulses)
{
    int n = 0;
    while(1)
    {
        if((motor_id == MOTOR_SELECT_1 && motor1_state) || 
           (motor_id == MOTOR_SELECT_2 && motor2_state) ||
           ((motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE) && (motor1_state || motor2_state)))
        {
             stepper_generate_pulse(motor_id);
             n++;
             if(n == pulses)
			 {
                 // 停止所有脉冲输出
                if(motor_id == MOTOR_SELECT_1 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
                {
                    pwm_set_duty(MOTOR4_TIMER, MOTOR4_GTIOC, 0);
                }
                
                if(motor_id == MOTOR_SELECT_2 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
                {
                    pwm_set_duty(MOTOR2_TIMER, MOTOR2_GTIOC, 0);
                }
                return true;
			 }
        }
    }
}

/**
 * 电机运动执行函数
 * @param motor_id: 电机选择，1=电机1，2=电机2，3=两个电机同方向，4=两个电机反方向
 * @param control_type: 控制类型，0=旋转控制，其他=角度控制
 * @param repeat_count: 执行次数
 * @param period_time: 每次执行的时间(ms)
 * @return: 执行成功返回true，失败返回false
 */
bool motor_execute(uint8_t motor_id, uint8_t control_type, uint16_t repeat_count, uint32_t period_time)
{
    printf("\r\n开始执行电机控制: 电机=%d, 类型=%d, 次数=%d, 周期=%dms", 
           motor_id, control_type, repeat_count, period_time);
    
    // 参数检查
    if(motor_id < MOTOR_SELECT_1 || motor_id > MOTOR_SELECT_OPPOSITE)
    {
        printf("\r\n错误: 无效的电机选择 (1-4)");
        return false;
    }
    
    if(repeat_count == 0)
    {
        printf("\r\n错误: 执行次数不能为0");
        return false;
    }
    
    if(period_time == 0)
    {
        printf("\r\n错误: 周期时间不能为0");
        return false;
    }
    
    // 根据控制类型执行不同操作
    if(control_type == CONTROL_ROTATION)  // 旋转控制
    {
        printf("\r\n执行旋转控制: 电机=%d, 旋转%d圈, 每圈%dms", 
               motor_id, repeat_count, period_time);
        
        // 设置电机方向为顺时针
        set_motor_direction(motor_id, STEPPER_DIR_CW);
        
        // 循环执行指定次数
        for(uint16_t i = 0; i < repeat_count; i++)
        {
            // 计算一圈需要的脉冲数 (360度)
            uint16_t pulses_per_rotation = 360 * PULSE_PER_DEGREE;
            
            // 调整电机速度以匹配指定的周期时间
            uint32_t target_speed = (pulses_per_rotation * 1000) / period_time;
            stepper_speed = target_speed;
            pulse_period = 1000 / stepper_speed;
            
            printf("\r\n第%d圈: 脉冲数=%d, 速度=%d Hz", i+1, pulses_per_rotation, stepper_speed);
            
            // 执行一圈
            motor_run_time(motor_id, period_time);
            
            // 两次旋转之间的短暂延时
            if(i < repeat_count - 1) 
            {
                system_delay_ms(100);
            }
        }
    }
    else  // 角度控制
    {
        float angle = (float)control_type;  // 角度值
        
        // 应用角度校准系数
        float corrected_angle1 = angle / angle_correction;   // 电机1校准角度
        float corrected_angle2 = angle / angle_correction2;  // 电机2校准角度
        
        printf("\r\n执行角度控制: 电机=%d, 目标角度=%.1f度", motor_id, angle);
        
        if(motor_id == MOTOR_SELECT_1 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
        {
            printf("\r\n电机1校准角度=%.1f度", corrected_angle1);
        }
        
        if(motor_id == MOTOR_SELECT_2 || motor_id == MOTOR_SELECT_BOTH || motor_id == MOTOR_SELECT_OPPOSITE)
        {
            printf("\r\n电机2校准角度=%.1f度", corrected_angle2);
        }
        
        printf("\r\n重复次数=%d, 每次周期=%dms", repeat_count, period_time);
        
        // 设置电机方向为顺时针
        set_motor_direction(motor_id, STEPPER_DIR_CW);
        
        // 循环执行指定次数
        for(uint16_t i = 0; i < repeat_count; i++)
        {
            // 计算所需脉冲数 (基于校准角度)
            uint16_t pulses1 = corrected_angle1 * PULSE_PER_DEGREE; // 电机1脉冲数
            uint16_t pulses2 = corrected_angle2 * PULSE_PER_DEGREE; // 电机2脉冲数
            
            // 取较大的脉冲数作为共同控制时的参考
            uint16_t pulses = (pulses1 > pulses2) ? pulses1 : pulses2;
            
            printf("\r\n第%d次执行: ", i+1);
            
            if(motor_id == MOTOR_SELECT_1)
            {
                printf("电机1脉冲数=%d", pulses1);
            }
            else if(motor_id == MOTOR_SELECT_2)
            {
                printf("电机2脉冲数=%d", pulses2);
            }
            else
            {
                printf("电机1脉冲数=%d, 电机2脉冲数=%d", pulses1, pulses2);
            }
            
            // 调整电机速度以匹配指定的周期时间
            uint32_t target_speed = (pulses * 1000) / period_time;
            stepper_speed = target_speed;
            pulse_period = 1000 / stepper_speed;
            
            // 执行角度旋转
            motor_run_time(motor_id, period_time);
            
            // 两次旋转之间的短暂延时
            if(i < repeat_count - 1) 
            {
                system_delay_ms(100);
            }
        }
    }
    
    printf("\r\n电机控制执行完成");
    return true;
}

/**
 * 设置角度校准系数
 * @param motor_id: 电机ID（1=电机1，2=电机2）
 * @param target_angle: 期望角度
 * @param actual_angle: 实际测量角度
 */
void set_angle_correction(uint8_t motor_id, float target_angle, float actual_angle)
{
    if(actual_angle <= 0 || target_angle <= 0)
    {
        printf("\r\n错误: 角度必须为正数");
        return;
    }
    
    if(motor_id == MOTOR_SELECT_1)
    {
        // 计算校准系数
        angle_correction = actual_angle / target_angle;
        printf("\r\n电机1角度校准系数更新: %.4f (目标角度=%.1f度, 实际角度=%.1f度)", 
               angle_correction, target_angle, actual_angle);
    }
    else if(motor_id == MOTOR_SELECT_2)
    {
        // 计算校准系数
        angle_correction2 = actual_angle / target_angle;
        printf("\r\n电机2角度校准系数更新: %.4f (目标角度=%.1f度, 实际角度=%.1f度)", 
               angle_correction2, target_angle, actual_angle);
    }
    else
    {
        printf("\r\n错误: 无效的电机ID");
    }
}

// 旧的电机控制函数，保留以兼容原有命令
void motor_control(const char cmd)
{
    switch(cmd)
    {
        case 'A':
            // 旋转90度，相当于 motor_execute(1, 90, 1, 1000)
            printf("\r\n执行原命令A: 电机1旋转90度");
            motor_execute(MOTOR_SELECT_1, 90, 1, 1000);
            break;
        case 'B':
            // 旋转45度，相当于 motor_execute(1, 45, 1, 800)
            printf("\r\n执行原命令B: 电机1旋转45度");
            motor_execute(MOTOR_SELECT_1, 45, 1, 800);
            break;
        case 'C':
            // 直行2秒，相当于 motor_execute(1, 0, 1, 2000)
            printf("\r\n执行原命令C: 电机1直行2秒");
            motor_execute(MOTOR_SELECT_1, 0, 1, 2000);
            break;
        default:
            printf("\r\n无效的电机命令");
            break;
    }
}

// ================== TCP通信函数声明 ==================
// 从tcp_uart.c引入函数
extern uint32_t get_system_time_ms(void);
extern uint8_t tcp_uart_init(void);
extern uint8_t tcp_uart_send_message(const char* message);

// 发送心跳包
static void tcp_send_heartbeat(void)
{
    if (!tcp_connected)
        return;
    
    char heartbeat_msg[] = "ping\r\n";
    uint32_t current_time = get_system_time_ms();
    
    // 检查是否需要发送心跳包（5秒一次）
    if (current_time - last_heartbeat_time >= HEARTBEAT_INTERVAL)
    {
        if (wifi_uart_send_buffer((uint8_t *)heartbeat_msg, strlen(heartbeat_msg)) == 0)
        {
            last_heartbeat_time = current_time;
            
            // 限制打印频率（每分钟打印一次）
            if (current_time - last_print_time >= PRINT_INTERVAL) {
                printf("\r\n发送心跳包成功");
                last_print_time = current_time;
            }
        }
        else
        {
            tcp_connected = 0;
            printf("\r\n发送心跳包失败，连接已断开");
        }
    }
}

// 解析接收到的消息
static void parse_received_message(const char* message)
{
    // 检查是否是IPD数据
    if (strstr(message, "+IPD,") != NULL)
    {
        // 提取真正的消息内容
        const char* msg_start = strstr(message, "msg=");
        if (msg_start != NULL)
        {
            // 移动到msg=后面
            msg_start += 4;
            
            // 查找消息结束位置（&uid或结尾）
            const char* msg_end = strstr(msg_start, "&uid=");
            if (msg_end == NULL) {
                msg_end = msg_start + strlen(msg_start);
            }
            
            // 计算消息长度并复制
            size_t msg_len = msg_end - msg_start;
            char* msg_part = (char*)malloc(msg_len + 1);
            if (msg_part == NULL) {
                printf("\r\n内存分配失败");
                return;
            }
            
            strncpy(msg_part, msg_start, msg_len);
            msg_part[msg_len] = '\0';
            
            // 检查主题是否匹配
            const char* topic_start = strstr(message, "topic=");
            if (topic_start != NULL) {
                topic_start += 6; // 跳过"topic="
                
                // 查找主题结束位置
                const char* topic_end = strstr(topic_start, "&");
                if (topic_end != NULL) {
                    size_t topic_len = topic_end - topic_start;
                    char topic[32] = {0};
                    
                    if (topic_len < sizeof(topic)) {
                        strncpy(topic, topic_start, topic_len);
                        topic[topic_len] = '\0';
                        
                        // 检查主题是否为"text1"
                        if (strcmp(topic, "text1") == 0) {
                            printf("\r\n收到主题[text1]的消息: %s", msg_part);
                        } else {
                            printf("\r\n收到主题[%s]的消息，不处理", topic);
                            free(msg_part);
                            return;
                        }
                    }
                }
            }
            
            // 拷贝干净的命令到消息缓冲区
        strncpy(message_buffer, msg_part, sizeof(message_buffer) - 1);
        message_buffer[sizeof(message_buffer) - 1] = '\0';
        
        // 设置新消息标志
        new_message_flag = 1;
        
            printf("\r\n接收到新指令: %s", message_buffer);
        
            // 处理命令
        process_motor_command(message_buffer);
            
            free(msg_part);
        }
    }
}

// 处理电机控制指令
void process_motor_command(const char* command)
{
    // 检查是否是组合指令（包含"|"分隔符）
    char command_copy[128];
    strncpy(command_copy, command, sizeof(command_copy) - 1);
    command_copy[sizeof(command_copy) - 1] = '\0';
    
    char* pipe_pos = strchr(command_copy, '|');
    if (pipe_pos != NULL) {
        // 这是一个组合指令
        *pipe_pos = '\0'; // 分割字符串
        char* first_cmd = command_copy;
        char* second_cmd = pipe_pos + 1;
        
        printf("\r\n识别到组合指令: 第一部分=%s, 第二部分=%s", first_cmd, second_cmd);
        
        // 先处理第一部分命令
        process_motor_command(first_cmd);
        // 等待第一个命令执行完成
        system_delay_ms(500);
        // 再处理第二部分命令
        process_motor_command(second_cmd);
        return;
    }
    
    // 如果当前正在执行指令，忽略新的指令（除了状态查询指令和前置指令）
    // 前置指令（left、right、back）即使在忙状态也要接收
    if (car_control_busy && 
        strcmp(command, "status") != 0 && 
        strcmp(command, "position()") != 0 &&
        strcmp(command, CMD_TURN_LEFT) != 0 &&
        strcmp(command, CMD_TURN_RIGHT) != 0 &&
        strcmp(command, CMD_TURN_BACK) != 0) {
        printf("\r\n小车正在执行控制指令，忽略新指令: %s", command);
        return;
    }
    
    // 处理前置指令（转向指令）
    if (strcmp(command, CMD_TURN_LEFT) == 0) {
        printf("\r\n收到左转指令");
        car_control_busy = 1; // 标记为正在执行指令
        turn_left();
        // 左转是前置指令，不需要立即上报位置
        car_control_busy = 0; // 重置忙状态标志
        return;
    }
    else if (strcmp(command, CMD_TURN_RIGHT) == 0) {
        printf("\r\n收到右转指令");
        car_control_busy = 1;
        turn_right();
        car_control_busy = 0;
        return;
    }
    else if (strcmp(command, CMD_TURN_BACK) == 0) {
        printf("\r\n收到掉头指令");
        car_control_busy = 1;
        turn_back();
        car_control_busy = 0;
        return;
    }
    // 处理控制指令（前进指令）
    else if (strcmp(command, CMD_FORWARD) == 0) {
        printf("\r\n收到前进1步指令");
        car_control_busy = 1;
        move_forward();
        // 前进后等待UWB位置更新，不再立即上报位置
        return;
    }
    // 处理前进多步指令：move:步数
    else if (strncmp(command, CMD_MOVE_STEPS, strlen(CMD_MOVE_STEPS)) == 0) {
        int steps = 0;
        if (sscanf(command, "move:%d", &steps) == 1 && steps > 0) {
            printf("\r\n收到前进%d步指令", steps);
            car_control_busy = 1;
            move_forward_steps(steps);
            // 前进后等待UWB位置更新，不再立即上报位置
        } else {
            printf("\r\n前进步数指令格式错误: %s", command);
        }
        return;
    }
    // 检查是否是初始化指令：init()
    else if(strcmp(command, "init()") == 0)
    {
        printf("\r\n执行小车位置初始化");
        // 初始化小车状态
        car_state.dir = DIR_NORTH;
        car_state_dir = DIR_NORTH;
        printf("\r\n小车朝向已重置为: %s", direction_names[car_state.dir]);
        // 立即上报位置
        report_position_to_cloud();
        return;
    }
    // 检查是否是位置查询指令：position()
    else if(strcmp(command, "position()") == 0)
    {
        extern int16 car_x_pos_filtered;
        extern int16 car_y_pos_filtered;
        
        // 更新当前状态
        car_state.x = car_x_pos_filtered;
        car_state.y = car_y_pos_filtered;
        
        printf("\r\n小车当前位置: (%d, %d)，朝向: %s", 
               car_state.x, car_state.y, direction_names[car_state.dir]);
        
        // 立即上报位置
        report_position_to_cloud();
        return;
    }
    // 检查是否是双基站位置查询指令：position2()
    else if(strcmp(command, "position2()") == 0)
    {
        extern int16 car_x_pos_filtered;
        extern int16 car_y_pos_filtered;
        
        // 更新当前状态
        car_state.x = car_x_pos_filtered;
        car_state.y = car_y_pos_filtered;
        
        printf("\r\n小车当前位置: (%d, %d)，朝向: %s", 
               car_state.x, car_state.y, direction_names[car_state.dir]);
        
        // 如果两个基站的数据都准备好了，直接发送双基站位置数据
        if (anchor1_data.data_ready && anchor2_data.data_ready) {
            // 立即上报位置，包含两个基站的数据
            char position_msg[256];
            snprintf(position_msg, sizeof(position_msg), 
                    "pos2:%d,%d,%d|%d,%d,%d,%d|%d,%d,%d,%d", 
                    car_state.x, car_state.y, (int)car_state.dir,
                    ANCHOR_ID_1, anchor1_data.distance, anchor1_data.azimuth, anchor1_data.elevation,
                    ANCHOR_ID_2, anchor2_data.distance, anchor2_data.azimuth, anchor2_data.elevation);
            
            // 发送位置信息到云平台
            if (tcp_uart_send_message(position_msg) == 0) {
                printf("\r\n上报双基站位置成功: %s", position_msg);
                printf("\r\n小车状态: 位置(%d,%d), 朝向:%s, 基站数:%d", 
                      car_state.x, car_state.y, direction_names[car_state.dir], anchor_count);
            }
        } else {
            printf("\r\n双基站数据尚未就绪，无法发送双基站位置数据");
            // 使用普通位置上报作为后备
            report_position_to_cloud();
        }
        return;
    }
    // 检查是否是mode指令：mode(电机,类型,次数,时间)
    else if(strncmp(command, "mode(", 5) == 0 && strlen(command) > 6)
    {
        // 解析参数
        uint8_t motor_id, control_type;
        uint16_t repeat_count;
        uint32_t period_time;

        // 修正格式指定符，使用%u而不是%lu以匹配uint32_t
        int matched = sscanf(command + 5, "%hhu,%hhu,%hu,%u)",
                             &motor_id, &control_type, &repeat_count, &period_time);

        if(matched == 4) // 成功匹配4个参数
        {
            printf("\r\n识别到电机控制指令: 电机=%d, 类型=%d, 次数=%d, 周期=%dms",
                   motor_id, control_type, repeat_count, period_time);
            car_control_busy = 1;
            motor_execute(motor_id, control_type, repeat_count, period_time);
            car_control_busy = 2;
            return;
        }
    }
    // 检查是否是calib指令：calib(电机,目标角度,实际角度)
    else if(strncmp(command, "calib(", 6) == 0 && strlen(command) > 7)
    {
        uint8_t motor_id;
        float target_angle, actual_angle;
        
        // 尝试解析参数
        int matched = sscanf(command + 6, "%hhu,%f,%f)", 
                           &motor_id, &target_angle, &actual_angle);
        
        if(matched == 3) // 成功匹配3个参数
        {
            printf("\r\n识别到校准指令: 电机=%d, 目标角度=%.1f度, 实际角度=%.1f度", 
                   motor_id, target_angle, actual_angle);
            set_angle_correction(motor_id, target_angle, actual_angle);
            return;
        }
    }
    // 检查是否是EXE指令：EXE:电机,类型,次数,时间
    else if(strncmp(command, "EXE:", 4) == 0)
    {
        uint8_t motor_id, control_type;
        uint16_t repeat_count;
        uint32_t period_time;

        // 修正格式指定符，使用%u而不是%lu以匹配uint32_t
        int matched = sscanf(command + 4, "%hhu,%hhu,%hu,%u",
                             &motor_id, &control_type, &repeat_count, &period_time);

        if(matched == 4)
        {
            printf("\r\n识别到EXE指令: 电机=%d, 类型=%d, 次数=%d, 周期=%dms",
                   motor_id, control_type, repeat_count, period_time);
            car_control_busy = 1;
            motor_execute(motor_id, control_type, repeat_count, period_time);
            car_control_busy = 2;
            return;
        }
    }
    // 检查是否是状态查询指令
    else if(strcmp(command, "status") == 0)
    {
        printf("\r\n电机1状态: EN=%d, STATE=%d", 
               gpio_get_level(STEPPER1_EN),
               motor1_state);
        printf("\r\n电机2状态: DIR=%d, STATE=%d", 
               gpio_get_level(STEPPER2_DIR),
               motor2_state);
        printf("\r\n校准参数: 脉冲系数=%.2f 脉冲/度", PULSE_PER_DEGREE);
        printf("\r\n电机1校准系数=%.4f, 电机2校准系数=%.4f", 
               angle_correction, angle_correction2);
        printf("\r\n控制状态: %s", car_control_busy ? "忙" : "空闲");
        printf("\r\n小车位置: (%d,%d), 朝向: %s", 
               car_state.x, car_state.y, direction_names[car_state.dir]);
        return;
    }
    
    // 不是电机控制指令，只做普通消息处理
    printf("\r\n非电机控制指令，作为普通消息处理");
}

extern uint8_t tcp_uart_get_new_message(char* buffer, uint16_t buffer_size);
extern void tcp_uart_process(void);

// ================== UWB相关定义 ==================
// 全局变量 - UWB相关
uint8 uart3_frame_buf[1024];      // UWB串口帧缓冲区
uint16 uart3_frame_len = 0;       // UWB串口帧长度
uint8 ff_count = 0;               // 连续0xFF的计数
bool uwb_frame_started = false;   // 是否已经开始接收新帧
uint32 last_uwb_print_time = 0;   // 上次打印UWB信息的时间
int16 car_x_pos = 0;              // 小车在UWB坐标系下的X坐标(单位格)
int16 car_y_pos = 0;              // 小车在UWB坐标系下的Y坐标(单位格)
int16 car_x_pos_filtered = 0;     // 滤波后的X坐标
int16 car_y_pos_filtered = 0;     // 滤波后的Y坐标
#define UWB_GRID_SIZE 15          // 每个坐标格对应15厘米
#define POS_FILTER_SIZE 5         // 位置滤波窗口大小
#define POS_FILTER_THRESHOLD 2    // 位置滤波阈值，超过此值才更新滤波结果
int16 pos_x_history[POS_FILTER_SIZE] = {0}; // X坐标历史值
int16 pos_y_history[POS_FILTER_SIZE] = {0}; // Y坐标历史值
uint8 pos_history_index = 0;      // 历史值索引
bool pos_filter_initialized = false; // 滤波器是否初始化

// UWB数据帧结构在tcp_uart.h中定义

// 定义UWB_Frame_t全局变量
UWB_Frame_t uwb_frame;            // UWB数据帧实例

// ================== UWB相关函数 ==================
// 从大端序的字节中读取16位无符号整数
uint16_t read_uint16_big_endian(uint8_t *data)
{
    return (data[0] << 8) | data[1];
}

// 从大端序的字节中读取32位无符号整数
uint32_t read_uint32_big_endian(uint8_t *data)
{
    return (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
}

// 从大端序的字节中读取16位有符号整数
int16_t read_int16_big_endian(uint8_t *data)
{
    return (int16_t)((data[0] << 8) | data[1]);
}

// 对坐标进行滤波处理
void filter_position(int16 new_x, int16 new_y) {
    // 更新历史值数组
    pos_x_history[pos_history_index] = new_x;
    pos_y_history[pos_history_index] = new_y;
    pos_history_index = (pos_history_index + 1) % POS_FILTER_SIZE;
    
    // 如果还没有收集足够的样本，不进行滤波
    if (!pos_filter_initialized) {
        if (pos_history_index == 0) {
            // 已收集POS_FILTER_SIZE个样本，初始化完成
            pos_filter_initialized = true;
        } else {
            // 初始化阶段，直接使用新值
            car_x_pos_filtered = new_x;
            car_y_pos_filtered = new_y;
            return;
        }
    }
    
    // 计算平均值
    int32 sum_x = 0, sum_y = 0;
    for (int i = 0; i < POS_FILTER_SIZE; i++) {
        sum_x += pos_x_history[i];
        sum_y += pos_y_history[i];
    }
    int16 avg_x = (int16)(sum_x / POS_FILTER_SIZE);
    int16 avg_y = (int16)(sum_y / POS_FILTER_SIZE);
    
    // 只有当新的平均值与当前滤波值差异超过阈值时才更新
    if (abs(avg_x - car_x_pos_filtered) > POS_FILTER_THRESHOLD ||
        abs(avg_y - car_y_pos_filtered) > POS_FILTER_THRESHOLD) {
        car_x_pos_filtered = avg_x;
        car_y_pos_filtered = avg_y;
    }
}

// 解析UWB数据帧
void parse_uwb_frame(uint8_t *data, uint16_t length) {
    if (length < 10) { // 至少需要帧头(4字节)+长度(2字节)+序列号(2字节)+命令(2字节)
        printf("\r\n[UWB] 数据不完整，长度不足");
        return;
    }
    
    // 解析命令类型 (大端序)
    uint16_t cmd = (data[8] << 8) | data[9];
    
    // 心跳包和位置数据包的共同部分解析
    uwb_frame.message_header = 0xFFFFFFFF;
    uwb_frame.packet_length = (data[4] << 8) | data[5]; // 大端序
    uwb_frame.sequence_id = (data[6] << 8) | data[7];   // 大端序
    uwb_frame.request_command = cmd;
    
    // 命令类型处理
    if (cmd == 0x2001) { // 位置数据包
        if (length < 37) {
            printf("\r\n[UWB] 位置数据包不完整，实际%d字节", length);
            return;
        }
        
        uwb_frame.version_id = (data[10] << 8) | data[11];
        uwb_frame.anchor_id = read_uint32_big_endian(&data[12]);
        uwb_frame.tag_id = read_uint32_big_endian(&data[16]);
        uwb_frame.distance = read_uint32_big_endian(&data[20]);
        uwb_frame.azimuth = read_int16_big_endian(&data[24]);
        uwb_frame.elevation = read_int16_big_endian(&data[26]);
        uwb_frame.tag_status = (data[28] << 8) | data[29];
        uwb_frame.batch_sn = (data[30] << 8) | data[31];
        uwb_frame.reserve = read_uint32_big_endian(&data[32]);
        
        // 跟踪基站ID
        uint32_t current_anchor_id = uwb_frame.anchor_id;
        bool is_new_anchor = true;
        
        // 检查是否是新的基站ID
        for (uint8_t i = 0; i < anchor_count; i++) {
            if (anchor_ids[i] == current_anchor_id) {
                is_new_anchor = false;
                break;
            }
        }
        
        // 如果是新的基站ID，记录到数组中
        if (is_new_anchor && anchor_count < MAX_ANCHOR_COUNT) {
            anchor_ids[anchor_count] = current_anchor_id;
            anchor_count++;
            
            // 如果基站数量大于1，标记为多基站模式
            if (anchor_count > 1) {
                is_multi_anchor = true;
            }
        }
        
        // 检查基站ID是否变化（多基站场景下）
        if (last_anchor_id != current_anchor_id) {
            last_anchor_id = current_anchor_id;
        }
        
        if (length >= 37) {
            uwb_frame.xor_byte = data[36];
            
            // 计算校验和
            uint8_t calc_checksum = 0;
            for (int i = 0; i < 36; i++) {
                calc_checksum ^= data[i];
            }
            
            // 校验
            if (calc_checksum != uwb_frame.xor_byte) {
                printf("\r\n[UWB] 校验错误");
                return;
            }
        }
        
        // 计算小车在UWB坐标系下的位置（以UWB基站为原点）
        // 距离单位是厘米，方位角是度数，需要转换为坐标
        float angle_rad = uwb_frame.azimuth * 3.14159f / 180.0f; // 角度转弧度
        
        // 修正后的坐标计算方式：
        // 如果方位角是从y轴（北方向）开始计算的
        // y = 距离 * cos(方位角)
        // x = 距离 * sin(方位角)
        float y_cm = uwb_frame.distance * cos(angle_rad);        // Y坐标(厘米)
        float x_cm = uwb_frame.distance * sin(angle_rad);        // X坐标(厘米)
        
        // 转换为坐标格（每15厘米一格）
        car_x_pos = (int16)(x_cm / UWB_GRID_SIZE);
        car_y_pos = (int16)(y_cm / UWB_GRID_SIZE);
        
        // 对坐标进行滤波处理
        filter_position(car_x_pos, car_y_pos);
        
        // 保存当前基站的数据
        uint32_t current_time = get_system_time();
        AnchorData *current_anchor = NULL;
        
        // 判断是否是特定的基站
        if (uwb_frame.anchor_id == ANCHOR_ID_1) {
            // 更新基站1数据
            current_anchor = &anchor1_data;
        } else if (uwb_frame.anchor_id == ANCHOR_ID_2) {
            // 更新基站2数据
            current_anchor = &anchor2_data;
        }
        
        // 如果是我们关注的基站，更新其数据
        if (current_anchor != NULL) {
            current_anchor->data_ready = true;
            current_anchor->distance = uwb_frame.distance;
            current_anchor->azimuth = uwb_frame.azimuth;
            current_anchor->elevation = uwb_frame.elevation;
            current_anchor->x_cm = x_cm;
            current_anchor->y_cm = y_cm;
            current_anchor->grid_x = car_x_pos;
            current_anchor->grid_y = car_y_pos;
            current_anchor->last_update_time = current_time;
            
            // 对该基站数据进行滤波
            filter_anchor_position(current_anchor);
            
            // 检查是否可以打印双基站数据
            bool can_print = false;
            
            // 当两个基站的数据都准备好，且距离上次打印已经过去0.5秒，才打印
            if (anchor1_data.data_ready && anchor2_data.data_ready && 
                (current_time - last_dual_print_time >= 500)) {
                can_print = true;
                last_dual_print_time = current_time;
                dual_anchor_print_ready = true;
            }
            
            // 打印双基站数据
            if (can_print) {
                printf("\r\n[UWB-双基站] 时间:%d", current_time);
                
                // 打印基站1数据
                printf("\r\n[UWB-1] 基站ID:0x%08X | 距离:%d cm | 方位:%d° | 仰角:%d° | 原始坐标:(%.1f,%.1f)cm | 网格坐标:(%d,%d) | 滤波后:(%d,%d)", 
                       ANCHOR_ID_1, anchor1_data.distance, anchor1_data.azimuth, anchor1_data.elevation, 
                       anchor1_data.x_cm, anchor1_data.y_cm, anchor1_data.grid_x, anchor1_data.grid_y, 
                       anchor1_data.filtered_x, anchor1_data.filtered_y);
                       
                // 打印基站2数据
                printf("\r\n[UWB-2] 基站ID:0x%08X | 距离:%d cm | 方位:%d° | 仰角:%d° | 原始坐标:(%.1f,%.1f)cm | 网格坐标:(%d,%d) | 滤波后:(%d,%d)", 
                       ANCHOR_ID_2, anchor2_data.distance, anchor2_data.azimuth, anchor2_data.elevation, 
                       anchor2_data.x_cm, anchor2_data.y_cm, anchor2_data.grid_x, anchor2_data.grid_y, 
                       anchor2_data.filtered_x, anchor2_data.filtered_y);
                       
                // 不合成位置，分别使用两个基站的数据
                // 注意：这里不再修改car_x_pos_filtered和car_y_pos_filtered，而是分别传输每个基站的数据
                
                // 打印带基站ID的完整数据，用于Python端解析
                printf("\r\n[UWB-数据] 基站1:0x%08X,%d,%d,%d,%d | 基站2:0x%08X,%d,%d,%d,%d", 
                       ANCHOR_ID_1, anchor1_data.distance, anchor1_data.azimuth, 
                       anchor1_data.grid_x, anchor1_data.grid_y,
                       ANCHOR_ID_2, anchor2_data.distance, anchor2_data.azimuth, 
                       anchor2_data.grid_x, anchor2_data.grid_y);
                
                // 如果正在等待位置更新，增加计数器
                if (waiting_for_position_update) {
                    position_update_counter++;
                    printf("\r\nUWB位置更新计数：%d/3", position_update_counter);
                    
                    // 当计数达到3次时，发送位置信息
                    if (position_update_counter >= 3) {
                        // 更新当前状态
                        car_state.x = car_x_pos_filtered;
                        car_state.y = car_y_pos_filtered;
                        
                        // 立即上报位置，包含两个基站的数据
                        char position_msg[256];
                        snprintf(position_msg, sizeof(position_msg), 
                                "pos2:%d,%d,%d|%d,%d,%d,%d|%d,%d,%d,%d", 
                                car_state.x, car_state.y, (int)car_state.dir,
                                ANCHOR_ID_1, anchor1_data.distance, anchor1_data.azimuth, anchor1_data.elevation,
                                ANCHOR_ID_2, anchor2_data.distance, anchor2_data.azimuth, anchor2_data.elevation);
                        
                        // 发送位置信息到云平台
                        if (tcp_uart_send_message(position_msg) == 0) {
                            printf("\r\n上报双基站位置成功: %s", position_msg);
                            printf("\r\n小车状态: 位置(%d,%d), 朝向:%s, 基站数:%d", 
                                  car_state.x, car_state.y, direction_names[car_state.dir], anchor_count);
                        }
                        
                        // 重置计数器和等待标志
                        position_update_counter = 0;
                        waiting_for_position_update = false;
                        
                        // 重置小车控制状态
                        car_control_busy = 0;
                    }
                }
            }
        } 
        else {
            // 对于其他基站，每0.5秒打印一次
        if (current_time - last_uwb_print_time >= 500) {
            last_uwb_print_time = current_time;
            
                // 打印UWB原始数据和计算的坐标位置，添加基站ID
                printf("\r\n[UWB-其他] 基站ID:0x%08X | 距离:%d cm | 方位:%d° | 仰角:%d° | 原始坐标:(%.1f,%.1f)cm | 网格坐标:(%d,%d) | 滤波后:(%d,%d)", 
                       uwb_frame.anchor_id, uwb_frame.distance, uwb_frame.azimuth, uwb_frame.elevation, 
                   x_cm, y_cm, car_x_pos, car_y_pos, car_x_pos_filtered, car_y_pos_filtered);
        }
        }
    } else if (cmd == 0x2002) { // 心跳包
        if (length < 16) {
            printf("\r\n[UWB] 心跳包不完整，实际%d字节", length);
            return;
        }
        
        uwb_frame.version_id = (data[10] << 8) | data[11];
        uwb_frame.anchor_id = read_uint32_big_endian(&data[12]);
        
        // 心跳包也显示基站ID
        printf("\r\n[UWB] 心跳包 | 基站ID:0x%08X | 序号:%d", 
              uwb_frame.anchor_id, uwb_frame.sequence_id);
    } else {
        printf("\r\n[UWB] 未知命令:0x%04X", cmd);
    }
}

// *************************** 例程硬件连接说明 ***************************
// 使用逐飞科技 CMSIS-DAP | ARM 调试下载器连接
//      直接将下载器正确连接在核心板的调试下载接口即可

// *************************** 例程测试说明 ***************************
// 1.核心板烧录完成本例程 将核心板插在主板上 插到底  将主板的串口2通过TTL连接到电脑上
// 
// 2.主板上电 或者核心板链接完毕后使用电池上电
// 
// 3.电脑上使用逐飞助手上位机，选择TTL的端口号
//
// 4.通过串口发送数据，数据将被转发到TCP服务器，同时接收从TCP服务器返回的数据
// **************************** 代码区域 ****************************

void hal_entry(void)
{
    init_sdram();               // 初始化所有引脚和SDRAM  全局变量后添加BSP_PLACE_IN_SECTION(".sdram")，将变量转到SDRAM中                         
    debug_init();               // 初始化Debug串口
    system_delay_ms(300);       // 等待主板其他外设上电完成
    
    printf("\r\n=== RA8D1 TCP与UWB控制演示程序 - 云平台控制版 ===");
    printf("\r\n功能说明:");
    printf("\r\n1. 小车将UWB定位坐标发送给云平台");
    printf("\r\n2. 云平台根据坐标计算路径并下发控制指令");
    printf("\r\n3. 小车执行指令后上报新的位置坐标");
    printf("\r\n4. 通过串口3接收UWB数据并解析位置信息");
    printf("\r\n5. UWB基站为坐标原点，每15厘米为一个坐标单位");
    printf("\r\n支持的云平台指令格式：");
    printf("\r\n1. %s - 左转90度", CMD_TURN_LEFT);
    printf("\r\n2. %s - 右转90度", CMD_TURN_RIGHT);
    printf("\r\n3. %s - 掉头180度", CMD_TURN_BACK);
    printf("\r\n4. %s - 前进一步", CMD_FORWARD);
    printf("\r\n5. %s:步数 - 前进指定步数", CMD_MOVE_STEPS);
    printf("\r\n6. status - 查询当前状态");
    printf("\r\n7. position() - 查询并上报当前位置");
    printf("\r\n8. position2() - 查询并上报当前位置及两个基站的详细信息");
    
    // 初始化定时器和步进电机
    pit_init();                 // 初始化定时器
    stepper_cr_vfcc_init();     // 初始化步进电机
    
    // 初始化TCP和串口
    tcp_uart_init();            // 初始化TCP通信
    uart3_init();               // 初始化串口3，用于UWB数据接收
    uart9_init();               // 初始化串口9，用于TCP通信
    
    // 初始化车辆状态
    car_state.x = 0;
    car_state.y = 0;
    car_state.dir = DIR_NORTH;
    car_state_dir = DIR_NORTH;
    car_control_busy = 0;
    
    printf("\r\n等待从串口3(引脚408/409)接收UWB数据...");
    printf("\r\n等待从串口9(引脚910/911)接收控制命令...");
    printf("\r\n系统准备就绪，等待云平台控制指令...");
    
    // 清空串口缓冲区
    uart3_rx_head = uart3_rx_tail = 0;
    uart9_rx_head = uart9_rx_tail = 0;
    uart9_frame_len = 0;
    frame_in_progress = false;
    
    // 初始化基站相关变量
    anchor_count = 0;
    last_anchor_id = 0;
    is_multi_anchor = false;
    for (uint8_t i = 0; i < MAX_ANCHOR_COUNT; i++) {
        anchor_ids[i] = 0;
    }
    
    // 初始化特定基站数据结构
    memset(&anchor1_data, 0, sizeof(AnchorData));
    memset(&anchor2_data, 0, sizeof(AnchorData));
    anchor1_data.data_ready = false;
    anchor2_data.data_ready = false;
    dual_anchor_print_ready = false;
    last_dual_print_time = 0;
    
    static uint32 last_check_time = 0;
    uint32 current_time = 0;
    static uint32 uwb_recv_count = 0;   // UWB接收数据计数
    static uint32 uwb_frame_count = 0;  // UWB接收帧计数
    uint16_t expected_length = 0;       // 期望的UWB数据长度
    uint16_t cmd_temp = 0;              // 临时存储UWB指令
    
    // 初始化时间变量 - 确保正确初始化
    last_check_time = get_system_time();
    
    // 注释掉TTS初始化和欢迎信息
    /*
    // 初始化TTS功能
    tts_init();
    
    printf("\r\n================小车通信系统================");
    printf("\r\n集成功能：");
    printf("\r\n1.UWB定位：通过串口9接收UWB位置数据");
    printf("\r\n2.TTS语音播报：支持通过串口3连接的TTS模块");
    printf("\r\n3.TCP通信：支持接收命令和发送状态信息");
    printf("\r\n4.文本转语音：收到tts:前缀的文本将转发到TTS模块");
    printf("\r\n5.支持长文本分段处理：格式为tts(m|n):");
    printf("\r\n6.TTS消息队列：支持缓存多条消息，顺序播放");
    printf("\r\n============================================\r\n");
    
    // 测试TTS功能 - 发送欢迎语
    char welcome_text[] = "系统初始化完成，欢迎使用";
    tts_queue_enqueue(welcome_text, strlen(welcome_text));
    process_tts_queue();
    */
    
    while (1)
    {
        // 处理TCP通信
        tcp_uart_process();
        
        // 检查是否有新的TCP消息
        if (tcp_uart_get_new_message(tcp_message_buffer, sizeof(tcp_message_buffer)))
        {
            // 将消息通过串口发送
            printf("\r\n转发TCP消息到串口: %s", tcp_message_buffer);
            g_uart9.p_api->write(g_uart9.p_ctrl, (uint8_t*)tcp_message_buffer, strlen(tcp_message_buffer));
            g_uart9.p_api->write(g_uart9.p_ctrl, (uint8_t*)"\r\n", 2); // 添加换行符
        }
        
        // 处理串口9接收到的数据（TCP命令）
        while (uart9_rx_tail != uart9_rx_head)
        {
            uint8_t data = uart9_rx_buf[uart9_rx_tail];
            uart9_rx_tail = (uart9_rx_tail + 1) % UART9_RX_BUF_SIZE;
            
            // 回显到串口
            g_uart9.p_api->write(g_uart9.p_ctrl, &data, 1);
            
            // 拼接可见字符到帧缓冲区
            if (data != '\n' && data != '\r' && uart9_frame_len < UART_FRAME_BUF_SIZE - 1)
            {
                uart9_frame_buf[uart9_frame_len++] = data;
                frame_in_progress = true;
            }
            
            // 如果遇到换行符，且当前有内容，认为一帧结束
            if ((data == '\n' || data == '\r') && frame_in_progress)
            {
                uart9_frame_buf[uart9_frame_len] = '\0'; // 添加字符串结尾
                
                // 发送到TCP服务器
                printf("\r\n转发串口消息到TCP: [%s], 长度: %d", uart9_frame_buf, uart9_frame_len);
                tcp_uart_send_message((char*)uart9_frame_buf);
                
                // 清空帧缓冲区
                uart9_frame_len = 0;
                frame_in_progress = false;
            }
        }
        
        // 注释掉TTS模块处理代码
        /*
        // 处理串口3接收到的数据（TTS模块）
        uint8_t temp_buffer[256];
        uint16_t read_len = tts_read_data(temp_buffer, sizeof(temp_buffer));
        
        if (read_len > 0) {
            // 收到TTS模块的回复
            memcpy(tts_response_buffer, temp_buffer, read_len);
            tts_response_len = read_len;
            tts_response_ready = 1;
            
            // 处理TTS模块回复
            process_tts_response();
        }
        */
        
        // 处理串口3接收到的UWB数据
        while (uart3_rx_tail != uart3_rx_head)
        {
            uint8_t data = uart3_rx_buf[uart3_rx_tail];
            uart3_rx_tail = (uart3_rx_tail + 1) % UART3_RX_BUF_SIZE;
            uwb_recv_count++;
            
            // 删除调试信息
            // if (uwb_recv_count % 100 == 0) {
            //     printf("\r\n[UWB-DEBUG] 已接收 %d 字节数据", uwb_recv_count);
            // }
            
            // 检测连续的0xFF (帧头特征)
            if (data == 0xFF) {
                ff_count++;
            } else {
                // 不是0xFF，重置计数
                ff_count = 0;
            }
            
            // 如果找到帧头（4个连续的0xFF）
            if (ff_count >= 4 && !uwb_frame_started) {
                uwb_frame_started = true;
                uart3_frame_len = 0;

                // 添加帧头到缓冲区
                for (int i = 0; i < 4; i++) {
                    uart3_frame_buf[uart3_frame_len++] = 0xFF;
                }
                
                // 重置，保证不会误判后续数据
                ff_count = 0;
                continue;  // 跳过将当前数据存入缓冲区
            }
            
            // 正常接收UWB数据
            if (uwb_frame_started && uart3_frame_len < sizeof(uart3_frame_buf)) {
                uart3_frame_buf[uart3_frame_len++] = data;
                
                // 当接收到帧头+2字节长度字段时，检查长度合理性
                if (uart3_frame_len == 6) {
                    uint8_t len_high = uart3_frame_buf[4];
                    uint8_t len_low = uart3_frame_buf[5];
                    
                    // 使用大端序解析长度字段
                    uint16_t packet_len = (len_high << 8) | len_low;
                    expected_length = packet_len;
                    
                    // 检查长度合理性 (位置数据包约37字节, 心跳包约16字节)
                    if (expected_length > 100) {
                        uwb_frame_started = false;
                        uart3_frame_len = 0;
                        ff_count = 0;
                        continue;
                    }
                }

                // 当接收到指令字段时，进一步判断
                if (uart3_frame_len == 10) {
                    cmd_temp = (uart3_frame_buf[8] << 8) | uart3_frame_buf[9]; // 大端序
                }
                
                // 判断是否接收完整帧
                if (uart3_frame_len >= 10) { // 至少接收到了命令字段
                    uint16_t cmd = (uart3_frame_buf[8] << 8) | uart3_frame_buf[9]; // 大端序
                    
                    // 根据命令类型判断应该接收的数据长度
                    if (cmd == 0x2001 && uart3_frame_len >= 37) { // 位置数据包约37字节
                        parse_uwb_frame(uart3_frame_buf, uart3_frame_len);
                        uwb_frame_count++;
                        
                        // 删除调试信息
                        // printf("\r\n[UWB-DEBUG] 成功解析位置数据帧 #%d，长度:%d字节", uwb_frame_count, uart3_frame_len);
                        
                        // 重置状态，准备接收下一帧
                        uwb_frame_started = false;
                        uart3_frame_len = 0;
                        ff_count = 0;
                    }
                    else if (cmd == 0x2002 && uart3_frame_len >= 16) { // 心跳包约16字节
                        parse_uwb_frame(uart3_frame_buf, uart3_frame_len);
                        uwb_frame_count++;
                        
                        // 删除调试信息
                        // printf("\r\n[UWB-DEBUG] 成功解析心跳数据帧 #%d，长度:%d字节", uwb_frame_count, uart3_frame_len);
                        
                        // 重置状态，准备接收下一帧
                        uwb_frame_started = false;
                        uart3_frame_len = 0;
                        ff_count = 0;
                    }
                }
                
                // 缓冲区溢出保护
                if (uart3_frame_len >= sizeof(uart3_frame_buf) - 1) {
                    // 重置状态，丢弃当前帧
                    uwb_frame_started = false;
                    uart3_frame_len = 0;
                    ff_count = 0;
                }
            }
        }
        
        // 每10秒打印一次UWB状态统计
        current_time = get_system_time();
        if (current_time - last_check_time >= 10000) 
        {
            last_check_time = current_time;
            printf("\r\n[UWB] 运行:%d秒 | 接收:%d字节 | %d帧 | 当前坐标:(%d,%d) | 基站数量:%d", 
                   current_time/1000, uwb_recv_count, uwb_frame_count,
                   car_x_pos_filtered, car_y_pos_filtered, anchor_count);
            
            // 如果是多基站模式，打印基站列表
            if (is_multi_anchor && anchor_count > 0) {
                printf("\r\n[UWB] 检测到多基站模式，基站列表：");
                for (uint8_t i = 0; i < anchor_count; i++) {
                    printf("0x%08X ", anchor_ids[i]);
                }
            }
            
            // 打印特定基站的状态
            printf("\r\n[UWB] 特定基站状态 - 基站1(0x%08X): %s | 基站2(0x%08X): %s", 
                   ANCHOR_ID_1, anchor1_data.data_ready ? "已接收" : "未接收",
                   ANCHOR_ID_2, anchor2_data.data_ready ? "已接收" : "未接收");
                   
            if (anchor1_data.data_ready && anchor2_data.data_ready) {
                printf("\r\n[UWB] 双基站合成位置: (%d,%d)", 
                       (anchor1_data.filtered_x + anchor2_data.filtered_x) / 2,
                       (anchor1_data.filtered_y + anchor2_data.filtered_y) / 2);
            }
        }
        
        system_delay_ms(10); // 短暂延时降低CPU占用
    }
}
// **************************** 代码区域 ****************************

// 向左转90度
void turn_left(void) {
    printf("\r\n执行左转操作...");
    
    // 修改左转逻辑，左边轮子向前，右边轮子向后 - 修正方向控制
    // 设置左右轮子不同方向
    gpio_set_level(STEPPER1_DIR, STEPPER_DIR_CCW);  // 左轮前进
    gpio_set_level(STEPPER2_DIR, STEPPER_DIR_CCW);  // 右轮后退
    
    // 使用相对方向控制(两轮反向)来实现更精确的转弯
    // 使用motor_id=4（相反方向控制两个电机）
    // 由于双轮反向转动效率提高，需要减少角度参数，原来90度的参数在双轮模式下调整为45度
    if (motor_execute(4, 45, 6, 200)) {  // 45度，6次，200ms/次
        printf("\r\n左转完成");
        
        // 更新朝向
        car_state.dir = (car_state.dir + 3) % 4;  // 左转相当于方向值-1（循环）
        car_state_dir = car_state.dir;
        printf("\r\n当前朝向: %s", direction_names[car_state.dir]);
    } else {
        printf("\r\n左转失败");
    }
}

// 向右转90度
void turn_right(void) {
    printf("\r\n执行右转操作...");
    
    // 右转逻辑，右边轮子向前，左边轮子向后
    gpio_set_level(STEPPER1_DIR, STEPPER_DIR_CW);  // 左轮后退
    gpio_set_level(STEPPER2_DIR, STEPPER_DIR_CW);  // 右轮前进
    
    // 使用相对方向控制(两轮反向)
    if (motor_execute(4, 45, 19, 75)) {  // 45度，6次，200ms/次
        printf("\r\n右转完成");
        
        // 更新朝向
        car_state.dir = (car_state.dir + 1) % 4;  // 右转相当于方向值+1
        car_state_dir = car_state.dir;
        printf("\r\n当前朝向: %s", direction_names[car_state.dir]);
    } else {
        printf("\r\n右转失败");
    }
}

// 掉头180度
void turn_back(void) {
    printf("\r\n执行掉头操作...");
    
    // 掉头逻辑（180度）- 为了精确，执行两次90度右转
    turn_left();
    system_delay_ms(500); // 短暂延时
    turn_left();
        
    printf("\r\n掉头完成，当前朝向: %s", direction_names[car_state.dir]);
}

// 前进一步
void move_forward(void) {
    printf("\r\n执行前进一步操作...");
    
    // 根据当前朝向决定前进方向
    switch(car_state.dir) {
        case DIR_NORTH: // 向北，Y+1
            // 执行向北移动
            motor_execute(3, 95, 3, 200);
            car_state.y += 1;
            break;
        case DIR_EAST: // 向东，X+1
            // 执行向东移动
            motor_execute(3, 95, 3, 200);
            car_state.x += 1;
            break;
        case DIR_SOUTH: // 向南，Y-1
            // 执行向南移动
            motor_execute(3, 95, 3, 200);
            car_state.y -= 1;
            break;
        case DIR_WEST: // 向西，X-1
            // 执行向西移动
            motor_execute(3, 95, 3, 200);
            car_state.x -= 1;
            break;
        }
        
    // 设置等待位置更新标志，启动位置更新计数
    waiting_for_position_update = true;
    position_update_counter = 0;
    printf("\r\n前进一步完成，等待UWB位置更新...");
    
    // 保持car_control_busy为1，直到位置更新计数满足条件
    car_control_busy = 1;
}

// 前进多步
void move_forward_steps(int steps) {
    printf("\r\n执行前进%d步操作...", steps);
    
    for (int i = 0; i < steps; i++) {
        printf("\r\n执行第%d/%d步...", i+1, steps);
        
        // 根据当前朝向决定前进方向
        switch(car_state.dir) {
            case DIR_NORTH: // 向北，Y+1
                // 执行向北移动
                motor_execute(3, 95, 3, 200);
                car_state.y += 1;
                break;
            case DIR_EAST: // 向东，X+1
                // 执行向东移动
                motor_execute(3, 95, 3, 200);
                car_state.x += 1;
                break;
            case DIR_SOUTH: // 向南，Y-1
                // 执行向南移动
                motor_execute(3, 95, 3, 200);
                car_state.y -= 1;
                break;
            case DIR_WEST: // 向西，X-1
                // 执行向西移动
                motor_execute(3, 95, 3, 200);
                car_state.x -= 1;
                break;
            }
        }
        
    // 设置等待位置更新标志，启动位置更新计数
    waiting_for_position_update = true;
    position_update_counter = 0;
    printf("\r\n前进%d步完成，等待UWB位置更新...", steps);
    
    // 保持car_control_busy为1，直到位置更新计数满足条件
    car_control_busy = 1;
}

// 向云平台上报当前位置
void report_position_to_cloud(void)
{
    // 如果在等待UWB位置更新，不执行常规上报
    if (waiting_for_position_update) {
        return;
    }
    
    extern int16 car_x_pos_filtered;
    extern int16 car_y_pos_filtered;
    
    // 获取当前时间
    uint32_t current_time = get_system_time_ms();
    
    // 检查是否需要上报位置（强制上报或定时上报）
    if ((current_time - last_position_report_time >= POSITION_INTERVAL) || 
        (car_control_busy == 2)) // 2表示操作刚结束，需要立即上报一次
    {
        // 更新当前状态（使用最新的UWB坐标）
        car_state.x = car_x_pos_filtered;
        car_state.y = car_y_pos_filtered;
        
        // 根据是否有双基站数据选择不同的上报格式
        if (anchor1_data.data_ready && anchor2_data.data_ready) {
            // 如果两个基站的数据都准备好了，发送完整的双基站信息
            char position_msg[256];
            snprintf(position_msg, sizeof(position_msg), 
                    "pos2:%d,%d,%d|%d,%d,%d,%d|%d,%d,%d,%d", 
                    car_state.x, car_state.y, (int)car_state.dir,
                    ANCHOR_ID_1, anchor1_data.distance, anchor1_data.azimuth, anchor1_data.elevation,
                    ANCHOR_ID_2, anchor2_data.distance, anchor2_data.azimuth, anchor2_data.elevation);
            
            // 发送位置信息到云平台
            if (tcp_uart_send_message(position_msg) == 0) {
                printf("\r\n上报双基站位置成功: %s", position_msg);
                printf("\r\n小车状态: 位置(%d,%d), 朝向:%s, 基站数:%d", 
                      car_state.x, car_state.y, direction_names[car_state.dir], anchor_count);
            }
        } else {
            // 兼容旧格式，只发送坐标信息
            char position_msg[128];
            // 组装位置信息，格式为"pos:x,y,dir"
            snprintf(position_msg, sizeof(position_msg), "pos:%d,%d,%d", 
                    car_state.x, car_state.y, (int)car_state.dir);
        
            // 发送位置信息到云平台
            if (tcp_uart_send_message(position_msg) == 0) {
                printf("\r\n上报位置成功: %s", position_msg);
                printf("\r\n小车当前位置: (%d, %d)，朝向: %s", 
                      car_state.x, car_state.y, direction_names[car_state.dir]);
            }
        }
    
        // 更新最后上报时间
        last_position_report_time = current_time;
    
        // 如果是操作结束后的上报，重置状态为空闲
        if (car_control_busy == 2) {
            car_control_busy = 0;
        }
    }
}

// 为特定基站数据进行滤波处理
void filter_anchor_position(AnchorData *anchor_data) {
    static const int16 max_delta = 3;       // 最大允许的变化量
    static const int16 filter_length = 5;   // 滤波队列长度
    
    // 初始化
    if (anchor_data->filtered_x == 0 && anchor_data->filtered_y == 0) {
        anchor_data->filtered_x = anchor_data->grid_x;
        anchor_data->filtered_y = anchor_data->grid_y;
        return;
    }
    
    // X坐标滤波
    int16 x_delta = anchor_data->grid_x - anchor_data->filtered_x;
    if (abs(x_delta) > max_delta) {
        // 变化过大，限制变化量
        x_delta = (x_delta > 0) ? max_delta : -max_delta;
    }
    anchor_data->filtered_x += x_delta / filter_length;
    
    // Y坐标滤波
    int16 y_delta = anchor_data->grid_y - anchor_data->filtered_y;
    if (abs(y_delta) > max_delta) {
        // 变化过大，限制变化量
        y_delta = (y_delta > 0) ? max_delta : -max_delta;
    }
    anchor_data->filtered_y += y_delta / filter_length;
}
