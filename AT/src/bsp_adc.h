#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "hal_data.h"

void ADC0_Init(void);
void ADC4_Init(void);
void ADC7_Init(void);
double Read_ADC_Voltage_Value(void);

#endif
