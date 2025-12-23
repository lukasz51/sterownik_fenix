#ifndef UART_CMD_H
#define UART_CMD_H

#include <stdint.h>

#define CMD_BUF_SIZE 128

uint8_t uart_cmd_push(const uint8_t *data, uint16_t len);
uint8_t uart_cmd_pop(uint8_t *out, uint16_t maxlen);

#endif
