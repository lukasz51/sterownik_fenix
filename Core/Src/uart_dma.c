#include "uart_dma.h"
#include "main.h"
#include "usart.h"
#include <string.h>

/* ================= KONFIG ================= */
#define UART_TX_BUF_SIZE 512
#define UART_RX_BUF_SIZE 256

extern UART_HandleTypeDef huart1;

/* ================= TX ================= */
static uint8_t  tx_buf[UART_TX_BUF_SIZE];
static volatile uint16_t tx_head = 0;
static volatile uint16_t tx_tail = 0;
static volatile uint8_t  tx_dma_busy = 0;

/* ================= RX ================= */
static uint8_t  rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

/* ================= INTERNAL ================= */
static void uart_tx_start_dma(void);

/* =========================================================
 *                         TX
 * ========================================================= */
void uart_tx_init(void)
{
    tx_head = tx_tail = 0;
    tx_dma_busy = 0;
}

uint8_t uart_tx_write(const uint8_t *data, uint16_t len)
{
    uint16_t free;

    if (tx_head >= tx_tail)
        free = UART_TX_BUF_SIZE - (tx_head - tx_tail) - 1;
    else
        free = tx_tail - tx_head - 1;

    if (len > free)
        return 0;

    for (uint16_t i = 0; i < len; i++)
    {
        tx_buf[tx_head++] = data[i];
        if (tx_head >= UART_TX_BUF_SIZE)
            tx_head = 0;
    }

    __disable_irq();
    if (!tx_dma_busy)
        uart_tx_start_dma();
    __enable_irq();

    return 1;
}

static void uart_tx_start_dma(void)
{
    if (tx_dma_busy || tx_head == tx_tail)
        return;

    tx_dma_busy = 1;

    uint16_t len;
    if (tx_head > tx_tail)
        len = tx_head - tx_tail;
    else
        len = UART_TX_BUF_SIZE - tx_tail;

    HAL_UART_Transmit_DMA(&huart1, &tx_buf[tx_tail], len);
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart != &huart1)
        return;

    tx_tail += huart->TxXferSize;
    if (tx_tail >= UART_TX_BUF_SIZE)
        tx_tail -= UART_TX_BUF_SIZE;

    tx_dma_busy = 0;
    uart_tx_start_dma();
}

/* =========================================================
 *                         RX
 * ========================================================= */
void uart_rx_init(void)
{
    rx_head = rx_tail = 0;

    HAL_UART_Receive_DMA(&huart1, rx_buf, UART_RX_BUF_SIZE);

    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);
}

uint16_t uart_rx_available(void)
{
    return (rx_head >= rx_tail)
           ? (rx_head - rx_tail)
           : (UART_RX_BUF_SIZE - rx_tail + rx_head);
}

uint16_t uart_rx_read(uint8_t *dst, uint16_t maxlen)
{
    uint16_t available = uart_rx_available();
    uint16_t to_read = (available < maxlen) ? available : maxlen;

    for (uint16_t i = 0; i < to_read; i++)
    {
        dst[i] = rx_buf[rx_tail++];
        if (rx_tail >= UART_RX_BUF_SIZE)
            rx_tail = 0;
    }

    return to_read;
}

/* ================= IDLE LINE ================= */
void HAL_UART_IDLE_Callback(UART_HandleTypeDef *huart)
{
    if (huart != &huart1)
        return;

    __HAL_UART_CLEAR_IDLEFLAG(huart);

    uint16_t dma_pos = UART_RX_BUF_SIZE -
                       __HAL_DMA_GET_COUNTER(huart->hdmarx);

    rx_head = dma_pos;
}
