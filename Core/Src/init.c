#include "delay_us.h"
#include "init.h"
#include "nrf24l01p.h"
#define RX_BUF_SIZE 20
extern uint32_t adc[8];
extern uint8_t uart_rx_byte;      // DMA odbiór po 1 bajcie



extern uint8_t rx_buf[RX_BUF_SIZE];


void init(void)
{
	HAL_Delay(1000);
	HAL_TIM_Base_Start_IT(&htim13);
	HAL_TIM_Base_Start_IT(&htim14);
    HAL_ADC_Start(&hadc1);
	HAL_ADC_Start_DMA(&hadc1, adc, 8);
	HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, RX_BUF_SIZE);
    nrf24l01p_rx_init(2476, _250kbps);
    delay_us_init();

}


