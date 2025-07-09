/*********************************************************************************************************************
* RA8D1 Opensourec Library （RA8D1 源码库）是一个基于RA8D1 芯片的 SDK 接口的硬件资源库
* Copyright (c) 2025 SEEKFREE 千如科技
* 
* 本文件是 RA8D1 源码库的一部分
* 
* RA8D1 源码库 包含以下内容
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
* 日期              修改人              备注
* 2025-03-25        ZSY            first version
********************************************************************************************************************/
#include "zf_driver_gpio.h"

//-------------------------------------------------------------------------------------------------------------------
//  @brief      GPIO设置
//  @param      pin         选择引脚 (可选范围：common.h中gpio_pin_enum枚举值确定)
//  @param      dat         0为低电平 1为高电平
//  @return     void
//  Sample usage:           gpio_set(B9, 1);//B9 设置高电平
//-------------------------------------------------------------------------------------------------------------------
void gpio_set_level(uint32 io, uint8 dat)
{
    if(dat) R_IOPORT_PinWrite(&g_ioport_ctrl, (bsp_io_port_pin_t)io, BSP_IO_LEVEL_HIGH);
    else    R_IOPORT_PinWrite(&g_ioport_ctrl, (bsp_io_port_pin_t)io, BSP_IO_LEVEL_LOW);
}

//-------------------------------------------------------------------------------------------------------------------
//  @brief      GPIO状态获取
//  @param      pin         选择引脚 (可选范围：common.h中gpio_pin_enum枚举值确定)
//  @return     uint8       0为低电平 1为高电平
//  Sample usage:           uint8 status = gpio_get(B9);//获取B9的低电平
//-------------------------------------------------------------------------------------------------------------------
uint8 gpio_get_level (uint32 io)
{
    bsp_io_level_t _level;
    R_IOPORT_PinRead(&g_ioport_ctrl, (bsp_io_port_pin_t)io, &_level);
    if(_level)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}
