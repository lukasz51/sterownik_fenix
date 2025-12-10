#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"
#include "cycle.h"
#include "usart.h"
#include "nextion_com.h"
#include <string.h>


#define RX_BUF_SIZE 20

uint8_t uart_rx_byte;      // DMA odbiór po 1 bajcie
uint8_t rx_buf[RX_BUF_SIZE];
volatile uint16_t rx_len = 0;

extern int temperature[4];
extern volatile  uint8_t enable_zone1;
extern volatile uint8_t enable_cwu;





void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{

	if (htim->Instance == TIM14)
		{

		  HAL_GPIO_TogglePin(MCU_LED_GPIO_Port, MCU_LED_Pin);
		  SendTemperatureNextion();


		}
}






void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if(huart->Instance == USART1)
    {
        rx_len = Size;  // liczba odebranych bajtów

        // Sprawdzenie, czy na końcu jest "cwp"
        if(rx_len >= 3 &&
           rx_buf[rx_len-3] == 'C' &&
           rx_buf[rx_len-2] == 'W' &&
           rx_buf[rx_len-1] == 'P')
        {
            // zakończ string przed "cwp"
            rx_buf[rx_len-3] = '\0';

            // Obsługa komend
            if(strcmp((char*)rx_buf, "10") == 0)
            {
                enable_zone1 = 0;
                enable_cwu   = 0;
            }
            else if(strcmp((char*)rx_buf, "11") == 0)
            {
                enable_zone1 = 1;
                enable_cwu   = 1;
            }
            else
            {
                // obsługa innych komend
            }

            // Kasowanie bufora po obsłudze
            memset(rx_buf, 0, RX_BUF_SIZE);
        }

        // Restart DMA do kolejnego odbioru
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, RX_BUF_SIZE);
    }
}




