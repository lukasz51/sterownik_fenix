#ifndef CYCLE_H
#define CYCLE_H

#include <stdint.h>
#include "nrf24l01p.h"

/* =========================================================
 *                TEMPERATURY (×10)
 * ========================================================= */
extern int temperature[4];

/* =========================================================
 *                ENABLE
 * ========================================================= */
extern volatile uint8_t enable_zone1;
extern volatile uint8_t enable_zone2;
extern volatile uint8_t enable_zone3;
extern volatile uint8_t enable_cwu;

/* =========================================================
 *                CYRKULACJA
 * ========================================================= */
extern volatile uint8_t enable_circulation;

/* =========================================================
 *                WYCHŁADZANIE KOTŁA
 * ========================================================= */
extern uint8_t  boiler_cooldown_active;
extern uint32_t boiler_cooldown_timer;

/* =========================================================
 *                SYSTEM
 * ========================================================= */
extern uint8_t system_ready;

/* =========================================================
 *                NRF RX BUFFER
 * ========================================================= */
extern uint8_t rx_data[NRF24L01P_PAYLOAD_LENGTH];

/* =========================================================
 *                API
 * ========================================================= */
void cycle(void);

#endif /* CYCLE_H */
