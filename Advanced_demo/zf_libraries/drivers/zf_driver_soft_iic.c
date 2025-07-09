/*********************************************************************************************************************
* RA8D1 Opensourec Library （RA8D1 源代码），一个基于RA8D1的SDK接口的源代码库
* Copyright (c) 2025 SEEKFREE （千如科技）
* 
* 本文件是 RA8D1 源代码的一部分
* 
* RA8D1 源代码 由以下部分组成：
* 1. 硬件初始化代码
* 2. 中断处理代码
* 3. 通讯代码
* 4. 其他代码
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
* 日期              修改人           备注
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "zf_driver_soft_iic.h"
#include "zf_driver_gpio.h"

#define SOFT_IIC_SDA_IO_SWITCH          (0)                                     // 是否需要 SDA 切换 I/O 口 0-不需要 1-需要

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     delay           用于延时
// 使用示例     soft_iic_delay(1);
// 备注信息     （内部使用）
//-------------------------------------------------------------------------------------------------------------------
#define soft_iic_delay(x)  for(volatile uint32_t i = x; i --; )


//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_start     产生IIC START信号
// 使用示例     soft_iic_start(soft_iic_obj);
// 备注信息     （内部使用）
//-------------------------------------------------------------------------------------------------------------------
static void soft_iic_start (soft_iic_info_struct *soft_iic_obj)
{
    gpio_high(soft_iic_obj->scl_pin);                                           // SCL 高电平
    gpio_high(soft_iic_obj->sda_pin);                                           // SDA 高电平

    soft_iic_delay(soft_iic_obj->delay);
    gpio_low(soft_iic_obj->sda_pin);                                            // SDA 低电平
    soft_iic_delay(soft_iic_obj->delay);
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平
    soft_iic_delay(soft_iic_obj->delay);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_stop     产生IIC STOP信号
// 使用示例     soft_iic_stop(soft_iic_obj);
// 备注信息     （内部使用）
//-------------------------------------------------------------------------------------------------------------------
static void soft_iic_stop (soft_iic_info_struct *soft_iic_obj)
{
    gpio_low(soft_iic_obj->sda_pin);                                            // SDA 低电平
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平

    soft_iic_delay(soft_iic_obj->delay);
    gpio_high(soft_iic_obj->scl_pin);                                           // SCL 高电平
    soft_iic_delay(soft_iic_obj->delay);
    gpio_high(soft_iic_obj->sda_pin);                                           // SDA 高电平
    soft_iic_delay(soft_iic_obj->delay);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_send_ack     产生IIC ACK/NAK信号 （内部使用）
// 使用示例     soft_iic_send_ack(soft_iic_obj, 1);
// 备注信息     （内部使用）
//-------------------------------------------------------------------------------------------------------------------
static void soft_iic_send_ack (soft_iic_info_struct *soft_iic_obj, uint8_t ack)
{
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平

    if(ack)
    {
        gpio_high(soft_iic_obj->sda_pin);                                       // SDA 高电平
    }
    else
    {
        gpio_low(soft_iic_obj->sda_pin);                                        // SDA 低电平
    }

    soft_iic_delay(soft_iic_obj->delay);
    gpio_high(soft_iic_obj->scl_pin);                                           // SCL 高电平
    soft_iic_delay(soft_iic_obj->delay);
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平
    gpio_high(soft_iic_obj->sda_pin);                                           // SDA 高电平
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_wait_ack     接收IIC ACK/NAK信号
// 使用示例     soft_iic_wait_ack(soft_iic_obj);
// 备注信息     （内部使用）
//-------------------------------------------------------------------------------------------------------------------
static uint8_t soft_iic_wait_ack (soft_iic_info_struct *soft_iic_obj)
{
    uint8_t temp = 0;
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平
    gpio_high(soft_iic_obj->sda_pin);                                           // SDA 高电平 拉低 SDA
#if SOFT_IIC_SDA_IO_SWITCH
    gpio_set_dir((gpio_pin_enum)soft_iic_obj->sda_pin, GPI, GPI_FLOATING_IN);
#endif
    soft_iic_delay(soft_iic_obj->delay);

    gpio_high(soft_iic_obj->scl_pin);                                           // SCL 高电平
    soft_iic_delay(soft_iic_obj->delay);

    if(gpio_get_level(soft_iic_obj->sda_pin))
    {
        temp = 1;
    }
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平
#if SOFT_IIC_SDA_IO_SWITCH
    gpio_set_dir((gpio_pin_enum)soft_iic_obj->sda_pin, GPO, GPO_OPEN_DTAIN);
#endif
    soft_iic_delay(soft_iic_obj->delay);

    return temp;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_send_data     发送IIC 8bit数据
// 使用示例     soft_iic_send_data(soft_iic_obj, 0x01);
// 备注信息     （内部使用）
//-------------------------------------------------------------------------------------------------------------------
static uint8_t soft_iic_send_data (soft_iic_info_struct *soft_iic_obj, const uint8_t data)
{
    uint8_t temp = 0x80;
    while(temp)
    {
//        gpio_set_level(soft_iic_obj->sda_pin, data & temp);
        ((data & temp) ? (gpio_high(soft_iic_obj->sda_pin)) : (gpio_low(soft_iic_obj->sda_pin)));
        temp >>= 1;

        soft_iic_delay(soft_iic_obj->delay / 2);
        gpio_high(soft_iic_obj->scl_pin);                                       // SCL 高电平
        soft_iic_delay(soft_iic_obj->delay);
        gpio_low(soft_iic_obj->scl_pin);                                        // SCL 低电平
        soft_iic_delay(soft_iic_obj->delay / 2);
    }
    return ((soft_iic_wait_ack(soft_iic_obj) == 1) ? 0 : 1 );
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_data     接收IIC 8bit数据
// 使用示例     soft_iic_read_data(soft_iic_obj, 1);
// 备注信息     （内部使用）
//-------------------------------------------------------------------------------------------------------------------
static uint8_t soft_iic_read_data (soft_iic_info_struct *soft_iic_obj, uint8_t ack)
{
    uint8_t data = 0x00;
    uint8_t temp = 8;
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平
    soft_iic_delay(soft_iic_obj->delay);
    gpio_high(soft_iic_obj->sda_pin);                                           // SDA 高电平 拉低 SDA
#if SOFT_IIC_SDA_IO_SWITCH
    gpio_set_dir((gpio_pin_enum)soft_iic_obj->sda_pin, GPI, GPI_FLOATING_IN);
#endif

    while(temp --)
    {
        gpio_low(soft_iic_obj->scl_pin);                                        // SCL 低电平
        soft_iic_delay(soft_iic_obj->delay);
        gpio_high(soft_iic_obj->scl_pin);                                       // SCL 高电平
        soft_iic_delay(soft_iic_obj->delay);
        data = (const uint8)(((data << 1) | gpio_get_level(soft_iic_obj->sda_pin)));
    }
    gpio_low(soft_iic_obj->scl_pin);                                            // SCL 低电平
#if SOFT_IIC_SDA_IO_SWITCH
    gpio_set_dir((gpio_pin_enum)soft_iic_obj->sda_pin, GPO, GPO_OPEN_DTAIN);
#endif
    soft_iic_delay(soft_iic_obj->delay);
    soft_iic_send_ack(soft_iic_obj, ack);
    return data;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_8bit     写入IIC 8bit数据
// 使用示例     soft_iic_write_8bit(soft_iic_obj, 0x01);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_8bit (soft_iic_info_struct *soft_iic_obj, const uint8_t data)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, data);
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_8bit_array     写入IIC 8bit数据数组
// 使用示例     soft_iic_write_8bit_array(soft_iic_obj, data, 6);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_8bit_array (soft_iic_info_struct *soft_iic_obj, const uint8_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    while(len --)
    {
        soft_iic_send_data(soft_iic_obj, *data ++);
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_16bit     写入IIC 16bit数据
// 使用示例     soft_iic_write_16bit(soft_iic_obj, 0x0101);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_16bit (soft_iic_info_struct *soft_iic_obj, const uint16_t data)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, (uint8_t)((data & 0xFF00) >> 8));
    soft_iic_send_data(soft_iic_obj, (uint8_t)(data & 0x00FF));
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_16bit_array     写入IIC 16bit数据数组
// 使用示例     soft_iic_write_16bit_array(soft_iic_obj, data, 6);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_16bit_array (soft_iic_info_struct *soft_iic_obj, const uint16_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    while(len --)
    {
        soft_iic_send_data(soft_iic_obj, (uint8_t)((*data & 0xFF00) >> 8));
        soft_iic_send_data(soft_iic_obj, (uint8_t)(*data ++ & 0x00FF));
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_8bit_register     写入IIC 8bit寄存器数据
// 使用示例     soft_iic_write_8bit_register(soft_iic_obj, 0x01, 0x01);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_8bit_register (soft_iic_info_struct *soft_iic_obj, const uint8_t register_name, const uint8_t data)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, register_name);
    soft_iic_send_data(soft_iic_obj, data);
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_8bit_registers     写入IIC 8bit寄存器数据数组
// 使用示例     soft_iic_write_8bit_registers(soft_iic_obj, 0x01, data, 6);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_8bit_registers (soft_iic_info_struct *soft_iic_obj, const uint8_t register_name, const uint8_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, register_name);
    while(len --)
    {
        soft_iic_send_data(soft_iic_obj, *data ++);
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_16bit_register     写入IIC 16bit寄存器数据
// 使用示例     soft_iic_write_16bit_register(soft_iic_obj, 0x0101, 0x0101);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_16bit_register (soft_iic_info_struct *soft_iic_obj, const uint16_t register_name, const uint16_t data)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, (uint8_t)((register_name & 0xFF00) >> 8));
    soft_iic_send_data(soft_iic_obj, (uint8_t)(register_name & 0x00FF));
    soft_iic_send_data(soft_iic_obj, (uint8_t)((data & 0xFF00) >> 8));
    soft_iic_send_data(soft_iic_obj, (uint8_t)(data & 0x00FF));
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_write_16bit_registers     写入IIC 16bit寄存器数据数组
// 使用示例     soft_iic_write_16bit_registers(soft_iic_obj, 0x0101, data, 6);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_write_16bit_registers (soft_iic_info_struct *soft_iic_obj, const uint16_t register_name, const uint16_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, (uint8_t)((register_name & 0xFF00) >> 8));
    soft_iic_send_data(soft_iic_obj, (uint8_t)(register_name & 0x00FF));
    while(len --)
    {
        soft_iic_send_data(soft_iic_obj, (uint8_t)((*data & 0xFF00) >> 8));
        soft_iic_send_data(soft_iic_obj, (uint8_t)(*data ++ & 0x00FF));
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_8bit     读取IIC 8bit数据
// 使用示例     soft_iic_read_8bit(soft_iic_obj);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
uint8_t soft_iic_read_8bit (soft_iic_info_struct *soft_iic_obj)
{
    uint8_t temp = 0;
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    temp = soft_iic_read_data(soft_iic_obj, 1);
    soft_iic_stop(soft_iic_obj);
    return temp;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_8bit_array     读取IIC 8bit数据数组
// 使用示例     soft_iic_read_8bit_array(soft_iic_obj, data, 8);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_read_8bit_array (soft_iic_info_struct *soft_iic_obj, uint8_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    while(len --)
    {
        *data ++ = soft_iic_read_data(soft_iic_obj, len == 0);
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_16bit     读取IIC 16bit数据
// 使用示例     soft_iic_read_16bit(soft_iic_obj);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
uint16_t soft_iic_read_16bit (soft_iic_info_struct *soft_iic_obj)
{
    uint16_t temp = 0;
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    temp = soft_iic_read_data(soft_iic_obj, 0);
    temp = (const uint8)(((temp << 8)| soft_iic_read_data(soft_iic_obj, 1)));
    soft_iic_stop(soft_iic_obj);
    return temp;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_16bit_array     读取IIC 16bit数据数组
// 使用示例     soft_iic_read_16bit_array(soft_iic_obj, data, 8);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_read_16bit_array (soft_iic_info_struct *soft_iic_obj, uint16_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    while(len --)
    {
        *data = soft_iic_read_data(soft_iic_obj, 0);
        *data = (const uint8)(((*data << 8)| soft_iic_read_data(soft_iic_obj, 0 == len)));
        data ++;
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_8bit_register     读取IIC 8bit寄存器数据
// 使用示例     soft_iic_read_8bit_register(soft_iic_obj, 0x01);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
uint8_t soft_iic_read_8bit_register (soft_iic_info_struct *soft_iic_obj, const uint8_t register_name)
{
    uint8_t temp = 0;
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, register_name);
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    temp = soft_iic_read_data(soft_iic_obj, 1);
    soft_iic_stop(soft_iic_obj);
    return temp;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_8bit_registers     读取IIC 8bit寄存器数据数组
// 使用示例     soft_iic_read_8bit_registers(soft_iic_obj, 0x01, data, 8);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_read_8bit_registers (soft_iic_info_struct *soft_iic_obj, const uint8_t register_name, uint8_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, register_name);
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    while(len --)
    {
        *data ++ = soft_iic_read_data(soft_iic_obj, len == 0);
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_16bit_register     读取IIC 16bit寄存器数据
// 使用示例     soft_iic_read_16bit_register(soft_iic_obj, 0x0101);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
uint16_t soft_iic_read_16bit_register (soft_iic_info_struct *soft_iic_obj, const uint16_t register_name)
{
    uint16_t temp = 0;
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, (uint8_t)((register_name & 0xFF00) >> 8));
    soft_iic_send_data(soft_iic_obj, (uint8_t)(register_name & 0x00FF));
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    temp = soft_iic_read_data(soft_iic_obj, 0);
    temp = (const uint8)(((temp << 8)| soft_iic_read_data(soft_iic_obj, 1)));
    soft_iic_stop(soft_iic_obj);
    return temp;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_read_16bit_registers     读取IIC 16bit寄存器数据数组
// 使用示例     soft_iic_read_16bit_registers(soft_iic_obj, 0x0101, data, 8);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_read_16bit_registers (soft_iic_info_struct *soft_iic_obj, const uint16_t register_name, uint16_t *data, uint32_t len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, (uint8_t)((register_name & 0xFF00) >> 8));
    soft_iic_send_data(soft_iic_obj, (uint8_t)(register_name & 0x00FF));
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    while(len --)
    {
        *data = soft_iic_read_data(soft_iic_obj, 0);
        *data = (const uint8)(((*data << 8)| soft_iic_read_data(soft_iic_obj, 0 == len)));
        data ++;
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_transfer_8bit_array     读写IIC 8bit数据数组
// 使用示例     iic_transfer_8bit_array(IIC_1, addr, data, 64, data, 64);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_transfer_8bit_array (soft_iic_info_struct *soft_iic_obj, const uint8_t *write_data, uint32_t write_len, uint8_t *read_data, uint32_t read_len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    while(write_len --)
    {
        soft_iic_send_data(soft_iic_obj, *write_data ++);
    }
    if(read_len)
    {
        soft_iic_start(soft_iic_obj);
        soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
        while(read_len --)
        {
            *read_data ++ = soft_iic_read_data(soft_iic_obj, 0 == read_len);
        }
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_transfer_16bit_array     读写IIC 16bit数据数组
// 使用示例     iic_transfer_16bit_array(IIC_1, addr, data, 64, data, 64);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_transfer_16bit_array (soft_iic_info_struct *soft_iic_obj, const uint16_t *write_data, uint32_t write_len, uint16_t *read_data, uint32_t read_len)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    while(write_len--)
    {
        soft_iic_send_data(soft_iic_obj, (uint8_t)((*write_data & 0xFF00) >> 8));
        soft_iic_send_data(soft_iic_obj, (uint8_t)(*write_data ++ & 0x00FF));
    }
    if(read_len)
    {
        soft_iic_start(soft_iic_obj);
        soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
        while(read_len --)
        {
            *read_data = soft_iic_read_data(soft_iic_obj, 0);
            *read_data = (const uint8)(((*read_data << 8) | soft_iic_read_data(soft_iic_obj, 0 == read_len)));
            read_data ++;
        }
    }
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_sccb_write_register     写入IIC SCCB模式 8bit寄存器数据
// 使用示例     soft_iic_sccb_write_register(soft_iic_obj, 0x01, 0x01);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_sccb_write_register (soft_iic_info_struct *soft_iic_obj, const uint8_t register_name, uint8_t data)
{
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, register_name);
    soft_iic_send_data(soft_iic_obj, data);
    soft_iic_stop(soft_iic_obj);
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_sccb_read_register     读取IIC SCCB模式 8bit寄存器数据
// 使用示例     soft_iic_sccb_read_register(soft_iic_obj, 0x01);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
uint8_t soft_iic_sccb_read_register (soft_iic_info_struct *soft_iic_obj, const uint8_t register_name)
{
    uint8_t temp = 0;
    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1));
    soft_iic_send_data(soft_iic_obj, register_name);
    soft_iic_stop(soft_iic_obj);

    soft_iic_start(soft_iic_obj);
    soft_iic_send_data(soft_iic_obj, (const uint8)(soft_iic_obj->addr << 1 | 0x01));
    temp = soft_iic_read_data(soft_iic_obj, 1);
    soft_iic_stop(soft_iic_obj);
    return temp;
}

//-------------------------------------------------------------------------------------------------------------------
// 函数说明     soft_iic_init     初始化IIC 提供MASTER模式 并提供SLAVE模式
// 使用示例     soft_iic_init(&soft_iic_obj, addr, 100, B6, B7);
// 备注信息
//-------------------------------------------------------------------------------------------------------------------
void soft_iic_init (soft_iic_info_struct *soft_iic_obj, uint8_t addr, uint32_t delay, bsp_io_port_pin_t scl_pin, bsp_io_port_pin_t sda_pin)
{
    soft_iic_obj->scl_pin = scl_pin;
    soft_iic_obj->sda_pin = sda_pin;
    soft_iic_obj->addr = addr;
    soft_iic_obj->delay = delay;
}
