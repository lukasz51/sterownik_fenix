#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

/* =========================================================
 *                SYSTEM TICK
 * ========================================================= */
extern volatile uint32_t sys_ms;

/* =========================================================
 *                FLAGI IRQ
 * ========================================================= */
extern uint8_t uart_tx_flag;
extern uint8_t nrf_rx_flag;
extern volatile uint8_t rf_flag;

#endif /* IRQ_H */
