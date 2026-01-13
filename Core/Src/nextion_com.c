
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "nrf24l01p.h"
#include "uart_dma.h"
#include <stdio.h>
#include <string.h>


#define RX_BUF_SIZE 20
int16_t t_room_nrf  = 0;   // ×10
int16_t t_room2_nrf = 0;   // placeholder
int16_t t_room3_nrf = 0;   // placeholder


extern uint8_t rx_data[RX_BUF_SIZE];
extern int temperature[4];

// statyczne bufory dla DMA — jeden na każdą wiadomość (bezpieczne dla DMA)
#define NEXTION_MSG_MAX 64
static uint8_t dma_buf[7][NEXTION_MSG_MAX];

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
   // if (i < NEXTION_MSG_MAX) outbuf[i++] = '.';
    //if (i < NEXTION_MSG_MAX) outbuf[i++] = '0' + frac;

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
    /* ===================== RX z NRF ===================== */
    if (rx_data[0] == 7 && rx_data[1] == 7)
    {
        /* pakiet synchronizacyjny – nic nie rób */
    }
    else
    {
        /* t_room z NRF (×10) */
        t_room_nrf = rx_data[0] * 10;
    }

    /* ===================== ROUND ROBIN ===================== */
    static uint8_t current_index = 0;

    const char *names[7] = {
        "tTemp2",
        "tTemp3",
        "tTemp4",
        "tTemp5",
        "t_room",
        "t_room2",
        "t_room3"
    };

    int temp_to_send = 0;

    switch (current_index)
    {
        /* ---- temperatury systemowe ---- */
        case 0: temp_to_send = temperature[0]; break;
        case 1: temp_to_send = temperature[1]; break;
        case 2: temp_to_send = temperature[2]; break;
        case 3: temp_to_send = temperature[3]; break;

        /* ---- temperatury pokojowe (NRF) ---- */
        case 4: temp_to_send = t_room_nrf;  break;
        case 5: temp_to_send = t_room2_nrf; break;   // przyszłość
        case 6: temp_to_send = t_room3_nrf; break;   // przyszłość
    }

    /* ===================== BUILD FRAME ===================== */
    int len = build_nextion_msg(
        names[current_index],
        temp_to_send,
        dma_buf[current_index]
    );

    if (len <= 0)
    {
        current_index++;
        if (current_index >= 7)
            current_index = 0;
        return;
    }

    /* ===================== TX BUFFER (DMA BACKGROUND) ===================== */
    uart_tx_write(dma_buf[current_index], (uint16_t)len);

    current_index++;
    if (current_index >= 7)
        current_index = 0;
}



