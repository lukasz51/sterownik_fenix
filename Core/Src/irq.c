#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"
#include "cycle.h"




void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

	if (htim->Instance == TIM14)
		{

		  HAL_GPIO_TogglePin(MCU_LED_GPIO_Port, MCU_LED_Pin);
		  SendTemperatureNextion();


		}
}
