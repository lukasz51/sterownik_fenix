
#include "init.h"

extern uint32_t adc[8];
void init(void)
{
	HAL_TIM_Base_Start_IT(&htim14);
    HAL_ADC_Start(&hadc1);
	HAL_ADC_Start_DMA(&hadc1, adc, 8);
}
