#include "gpio_config.h"
#include "asr_ctrl.h" // 添加包含KEY4定义的头文件

// GPIO引脚名称数组 - 确保足够大以容纳KEY4的索引值
const char* gpio_pin_name[] = {
    "未知引脚",
    "KEY1", 
    "KEY2", 
    "KEY3", 
    "KEY4",    // ASR_BUTTON 使用的引脚
    "KEY5",
    "KEY6",
    "KEY7",
    "KEY8"
};

// GPIO初始化函数实现
void gpio_init(uint32_t pin, gpio_mode_enum mode, gpio_level_enum level, gpio_pull_enum pull)
{
    // 调试输出
    printf("\r\nGPIO初始化: 引脚值=%d, 模式=%d, 电平=%d, 上下拉=%d", pin, mode, level, pull);
    
    // 判断是否为语音识别按钮
    if (pin == ASR_BUTTON) {
        printf("\r\n配置语音识别按钮：KEY4 (BSP_IO_PORT_11_PIN_05)");
        
        // 配置为输入模式
        if (mode == GPI) {
            // 使用R_IOPORT_PinCfg配置引脚
            if (pull == GPI_PULL_UP) {
                // 上拉配置
                R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_11_PIN_05, 
                                IOPORT_CFG_PORT_DIRECTION_INPUT | IOPORT_CFG_PULLUP_ENABLE);
            } else {
                // 由于没有下拉选项，使用浮空输入
                R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_11_PIN_05, 
                                IOPORT_CFG_PORT_DIRECTION_INPUT);
            }
            printf("\r\nKEY4按键 已初始化为输入模式");
        } 
        // 配置为输出模式
        else if (mode == GPO) {
            // 使用R_IOPORT_PinCfg配置引脚
            R_IOPORT_PinCfg(&g_ioport_ctrl, BSP_IO_PORT_11_PIN_05, IOPORT_CFG_PORT_DIRECTION_OUTPUT);
            // 设置输出电平
            R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_11_PIN_05, (level == GPIO_HIGH) ? BSP_IO_LEVEL_HIGH : BSP_IO_LEVEL_LOW);
            printf("\r\nKEY4按键 已初始化为输出模式");
        }
    }
    else {
        // 安全检查，确保pin索引值在合理范围内（用作数组索引）
        if (pin >= 0 && pin < sizeof(gpio_pin_name)/sizeof(char*)) {
            printf("\r\nGPIO引脚 %s 已初始化", gpio_pin_name[pin]);
        }
        else {
            printf("\r\n错误: 引脚值超出范围! pin=%d", pin);
        }
    }
} 