
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

#include <stdio.h>
#include <string.h>

extern int temperature[4];

// statyczne bufory dla DMA — jeden na każdą wiadomość (bezpieczne dla DMA)
#define NEXTION_MSG_MAX 32
static uint8_t dma_buf[4][NEXTION_MSG_MAX];

static int build_nextion_msg(const char *comp, int temp, uint8_t *outbuf)
{
    // budujemy: comp.txt="xx.xC" + 0xFF 0xFF 0xFF
    int i = 0;

    // component name i ".txt="
    const char *p = comp;
    while (*p && i < NEXTION_MSG_MAX) outbuf[i++] = *p++;
    const char suffix[] = ".txt=\"";
    for (int k = 0; k < (int)sizeof(suffix)-1 && i < NEXTION_MSG_MAX; k++)
        outbuf[i++] = suffix[k];

    // temp: temp jest w dziesiątych (np. 245 -> 24.5)
    int negative = 0;
    if (temp < 0) { negative = 1; temp = -temp; }

    int integer = temp / 10;
    int frac = temp % 10;

    if (negative && i < NEXTION_MSG_MAX) outbuf[i++] = '-';

    // część całkowita -> zapis bez sprintf
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
    // odwróć
    for (int j = idx-1; j >= 0 && i < NEXTION_MSG_MAX; j--) outbuf[i++] = numbuf[j];

    // kropka i część ułamkowa
    if (i < NEXTION_MSG_MAX) outbuf[i++] = '.';
    if (i < NEXTION_MSG_MAX) outbuf[i++] = '0' + (frac % 10);

    // 'C' i zamknięcie cudzysłowu
    if (i < NEXTION_MSG_MAX) outbuf[i++] = 'C';
    if (i < NEXTION_MSG_MAX) outbuf[i++] = '"';

    // trzy bajty końca Nextion
    if (i + 3 <= NEXTION_MSG_MAX) {
        outbuf[i++] = 0xFF;
        outbuf[i++] = 0xFF;
        outbuf[i++] = 0xFF;
    } else {
        // za mało miejsca (nie powinno się zdarzyć)
        return -1;
    }

    return i; // długość komunikatu
}

void SendTemperatureNextion(void)
{
    static uint8_t current_index = 0;

    // nazwy komponentów odpowiadające indexom
    const char *names[4] = { "tTemp", "tTemp2", "tTemp3", "tTemp4" };

    // przygotuj wiadomość do statycznego bufora
    int len = build_nextion_msg(names[current_index], temperature[current_index], dma_buf[current_index]);
    if (len <= 0) {
        // błąd budowania - pomiń i przejdź dalej
        current_index = (current_index + 1) & 3;
        return;
    }

    // sprawdź czy UART jest gotowy do nowej transmisji DMA
    if (HAL_UART_GetState(&huart1) == HAL_UART_STATE_READY) {
        HAL_StatusTypeDef st = HAL_UART_Transmit_DMA(&huart1, dma_buf[current_index], len);
        if (st == HAL_OK) {
            // tylko wtedy przechodzimy do następnego indeksu — w przeciwnym razie spróbujemy ponownie następnym razem
            current_index++;
            if (current_index >= 4) current_index = 0;
        } else {
            // HAL zwrócił BUSY lub ERROR — nie zmieniamy indexu, spróbujemy ponownie następnym wywołaniem
        }
    } else {
        // UART zajęty — nic nie robimy, spróbujemy ponownie przy kolejnym wywołaniu
    }
}
