#include "uart_cmd.h"
#include <string.h>
#include "main.h"

static uint8_t  cmd_buf[CMD_BUF_SIZE];
static uint16_t cmd_head = 0;
static uint16_t cmd_tail = 0;



uint8_t uart_cmd_push(const uint8_t *data, uint16_t len)
{
    if (len > CMD_BUF_SIZE)
        return 0;

    __disable_irq();

    uint16_t free = (cmd_head >= cmd_tail)
        ? (CMD_BUF_SIZE - (cmd_head - cmd_tail) - 1)
        : (cmd_tail - cmd_head - 1);

    if (len > free)
    {
        __enable_irq();
        return 0;
    }

    for (uint16_t i = 0; i < len; i++)
    {
        cmd_buf[cmd_head++] = data[i];
        if (cmd_head >= CMD_BUF_SIZE)
            cmd_head = 0;
    }

    __enable_irq();
    return 1;
}

uint8_t uart_cmd_pop(uint8_t *out, uint16_t maxlen)
{
    uint16_t len = 0;

    __disable_irq();

    while (cmd_tail != cmd_head && len < maxlen)
    {
        out[len++] = cmd_buf[cmd_tail++];
        if (cmd_tail >= CMD_BUF_SIZE)
            cmd_tail = 0;
    }

    __enable_irq();
    return len;
}
