#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "uart_dma.h"
#include <stdio.h>
#include <string.h>

/* =========================================================
 *                EXTERNY Z cycle.c
 * ========================================================= */
extern int temperature[4];
extern uint8_t t_room1;
extern uint8_t t_room2;
extern uint8_t t_room3;

/* =========================================================
 *                NEXTION DMA
 * ========================================================= */
#define NEXTION_MSG_MAX 64
static uint8_t dma_buf[7][NEXTION_MSG_MAX];

/* =========================================================
 *                BUILD MESSAGE
 * ========================================================= */
static int build_nextion_msg(const char *comp, int temp, uint8_t *outbuf)
{
    /* nie wysyłamy ujemnych */
    if (temp < 0)
        return -1;

    int i = 0;

    /* component name */
    const char *p = comp;
    while (*p && i < NEXTION_MSG_MAX)
        outbuf[i++] = *p++;

    const char suffix[] = ".txt=\"";
    for (int k = 0; k < (int)sizeof(suffix) - 1 && i < NEXTION_MSG_MAX; k++)
        outbuf[i++] = suffix[k];

    /* temp w ×10 */
    int integer = temp / 10;

    char numbuf[8];
    int idx = 0;

    if (integer == 0)
        numbuf[idx++] = '0';
    else
    {
        int t = integer;
        while (t > 0 && idx < (int)sizeof(numbuf))
        {
            numbuf[idx++] = '0' + (t % 10);
            t /= 10;
        }
    }

    for (int j = idx - 1; j >= 0 && i < NEXTION_MSG_MAX; j--)
        outbuf[i++] = numbuf[j];

    if (i < NEXTION_MSG_MAX) outbuf[i++] = 0xB0;  // °

    // litera C
    if (i < NEXTION_MSG_MAX) outbuf[i++] = 'C';
    if (i < NEXTION_MSG_MAX) outbuf[i++] = '"';

    if (i + 3 <= NEXTION_MSG_MAX)
    {
        outbuf[i++] = 0xFF;
        outbuf[i++] = 0xFF;
        outbuf[i++] = 0xFF;
    }
    else
        return -1;

    return i;
}

/* =========================================================
 *                SEND TO NEXTION
 * ========================================================= */
#define NEXTION_FIELDS 7

void SendTemperatureNextion(void)
{
    static uint8_t current_index = 0;

    /* normalizacja indeksu – GCC to rozumie */
    current_index %= NEXTION_FIELDS;

    static const char *names[NEXTION_FIELDS] =
    {
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
        case 0: temp_to_send = temperature[0]; break;
        case 1: temp_to_send = temperature[1]; break;
        case 2: temp_to_send = temperature[2]; break;
        case 3: temp_to_send = temperature[3]; break;

        case 4: temp_to_send = t_room1 * 10; break;
        case 5: temp_to_send = t_room2 * 10; break;
        case 6: temp_to_send = t_room3 * 10; break;
    }

    int len = build_nextion_msg(
        names[current_index],
        temp_to_send,
        dma_buf[current_index]
    );

    if (len > 0)
        uart_tx_write(dma_buf[current_index], (uint16_t)len);

    current_index++;
}
