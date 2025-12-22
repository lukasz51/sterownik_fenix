
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "nrf24l01p.h"
#include <stdio.h>
#include <string.h>


#define RX_BUF_SIZE 20



extern uint8_t rx_data[RX_BUF_SIZE];
extern int temperature[4];

// statyczne bufory dla DMA — jeden na każdą wiadomość (bezpieczne dla DMA)
#define NEXTION_MSG_MAX 32
static uint8_t dma_buf[4][NEXTION_MSG_MAX];

static int build_nextion_msg(const char *comp, int temp, uint8_t *outbuf)
{
    // ❌ nie wysyłamy ujemnych temperatur
    if (temp < 0) {
        return -1;
    }

    // budujemy: comp.txt="xx.xC" + 0xFF 0xFF 0xFF
    int i = 0;

    // component name i ".txt="
    const char *p = comp;
    while (*p && i < NEXTION_MSG_MAX) outbuf[i++] = *p++;
    const char suffix[] = ".txt=\"";
    for (int k = 0; k < (int)sizeof(suffix)-1 && i < NEXTION_MSG_MAX; k++)
        outbuf[i++] = suffix[k];

    // temp jest w dziesiątych (np. 245 -> 24.5)
    int integer = temp / 10;
    int frac = temp % 10;

    // część całkowita
    char numbuf[8];
    int idx = 0;
    if (integer == 0) numbuf[idx++] = '0';
    else {
        int t = integer;
        while (t > 0 && idx < (int)sizeof(numbuf)) {
            numbuf[idx++] = '0' + (t % 10);
            t /= 10;
        }
    }

    for (int j = idx-1; j >= 0 && i < NEXTION_MSG_MAX; j--)
        outbuf[i++] = numbuf[j];

    // część ułamkowa
    if (i < NEXTION_MSG_MAX) outbuf[i++] = '.';
    if (i < NEXTION_MSG_MAX) outbuf[i++] = '0' + frac;

    // 'C' i zamknięcie
    if (i < NEXTION_MSG_MAX) outbuf[i++] = 'C';
    if (i < NEXTION_MSG_MAX) outbuf[i++] = '"';

    // terminator Nextion
    if (i + 3 <= NEXTION_MSG_MAX) {
        outbuf[i++] = 0xFF;
        outbuf[i++] = 0xFF;
        outbuf[i++] = 0xFF;
    } else {
        return -1;
    }

    return i;
}


void SendTemperatureNextion(void)
{
	if (rx_data[0] == 7 && rx_data[1] == 7)
	{
	    // wykryto pakiet 7,7
	}
	else
	{
		temperature[2] = (rx_data[0]*10 + rx_data[1]);
	}

				static uint8_t current_index = 0;

    // nazwy komponentów odpowiadające indexom
    const char *names[4] = { "tTemp2", "tTemp3", "t_room", "tTemp4" };

    // przygotuj wiadomość do statycznego bufora
    int len = build_nextion_msg(names[current_index], temperature[current_index], dma_buf[current_index]);
    if (len <= 0) {
        // błąd budowania - pomiń i przejdź dalej
        current_index = (current_index + 1) & 3;
        return;
    }
        HAL_UART_Transmit_DMA(&huart1, dma_buf[current_index], len);
        current_index++;
        if (current_index >= 4) current_index = 0;


}

