#include "bsp_adc.h"

//ADC转换完成标志位
volatile bool scan_complete_flag = false;

void adc_callback(adc_callback_args_t * p_args)
{
    FSP_PARAMETER_NOT_USED(p_args);
    scan_complete_flag = true;
}

void ADC0_Init(void)
{
    fsp_err_t err;
    err = R_ADC_Open(&g_adc0_ctrl, &g_adc0_cfg);
    err = R_ADC_ScanCfg(&g_adc0_ctrl, &g_adc0_channel_cfg);

    assert(FSP_SUCCESS == err);
}
void ADC4_Init(void)
{
    fsp_err_t err;
    err = R_ADC_Open(&g_adc4_ctrl, &g_adc4_cfg);
    err = R_ADC_ScanCfg(&g_adc4_ctrl, &g_adc4_channel_cfg);

    assert(FSP_SUCCESS == err);
}
void ADC7_Init(void)
{
    fsp_err_t err;
    err = R_ADC_Open(&g_adc7_ctrl, &g_adc7_cfg);
    err = R_ADC_ScanCfg(&g_adc7_ctrl, &g_adc7_channel_cfg);

    assert(FSP_SUCCESS == err);
}

uint16_t adc0_pro(void)
{
    fsp_err_t err;
    uint16_t channel1_conversion_result;
    /* Wait for conversion to complete. */
      adc_status_t status;
      (void) R_ADC_ScanStart(&g_adc0_ctrl);
      status.state = ADC_STATE_SCAN_IN_PROGRESS;
      while (ADC_STATE_SCAN_IN_PROGRESS == status.state)
      {
          (void) R_ADC_StatusGet(&g_adc0_ctrl, &status);
      }
      /* Read converted data. */

      err = R_ADC_Read(&g_adc0_ctrl, ADC_CHANNEL_0, &channel1_conversion_result);
      assert(FSP_SUCCESS == err);
      return channel1_conversion_result;
}
uint16_t adc4_pro(void)
{
    fsp_err_t err;
    uint16_t channel1_conversion_result;
    /* Wait for conversion to complete. */
      adc_status_t status;
      (void) R_ADC_ScanStart(&g_adc4_ctrl);
      status.state = ADC_STATE_SCAN_IN_PROGRESS;
      while (ADC_STATE_SCAN_IN_PROGRESS == status.state)
      {
          (void) R_ADC_StatusGet(&g_adc4_ctrl, &status);
      }
      /* Read converted data. */

      err = R_ADC_Read(&g_adc4_ctrl, ADC_CHANNEL_4, &channel1_conversion_result);
      assert(FSP_SUCCESS == err);
      return channel1_conversion_result;
}
uint16_t adc7_pro(void)
{
    fsp_err_t err;
    uint16_t channel1_conversion_result;
    /* Wait for conversion to complete. */
      adc_status_t status;
      (void) R_ADC_ScanStart(&g_adc7_ctrl);
      status.state = ADC_STATE_SCAN_IN_PROGRESS;
      while (ADC_STATE_SCAN_IN_PROGRESS == status.state)
      {
          (void) R_ADC_StatusGet(&g_adc7_ctrl, &status);
      }
      /* Read converted data. */

      err = R_ADC_Read(&g_adc7_ctrl, ADC_CHANNEL_7, &channel1_conversion_result);
      assert(FSP_SUCCESS == err);
      return channel1_conversion_result;
}
/* 进行ADC采集，读取ADC数据并转换结果 */
double Read_ADC_Voltage_Value(void)
{
    uint16_t adc_data;
    double a0;
    scan_complete_flag = false; //重新清除标志位
    (void)R_ADC_ScanStart(&g_adc0_ctrl);
    while (!scan_complete_flag) //等待转换完成标志
    {
        ;
    }


    /* 读取通道0数据 */
    R_ADC_Read(&g_adc0_ctrl, ADC_CHANNEL_0, &adc_data);
    /* ADC原始数据转换为电压值（ADC参考电压为3.3V） */
    a0 = (double)(adc_data*3.3/4095);

    return a0;
}
