#ifndef UART_DMA_H
#define UART_DMA_H

#include <stdint.h>

/* ================= TX ================= */
void uart_tx_init(void);
uint8_t uart_tx_write(const uint8_t *data, uint16_t len);

/* ================= RX ================= */
void uart_rx_init(void);
uint16_t uart_rx_available(void);
uint16_t uart_rx_read(uint8_t *dst, uint16_t maxlen);

#endif
