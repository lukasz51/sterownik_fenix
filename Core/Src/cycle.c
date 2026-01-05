#include "cycle.h"
#include "heat.h"
#include "irq.h"

#include "main.h"
#include "thermistor.h"
#include "nextion_com.h"
#include "uart_cmd.h"
#include "nrf24l01p.h"
#include <stdlib.h>
#include <string.h>

/* =========================================================
 *                EXTERN Z IRQ
 * ========================================================= */
extern uint8_t uart_tx_flag;
extern uint8_t nrf_rx_flag;
extern volatile uint8_t rf_flag;
extern uint32_t adc[8];

/* =========================================================
 *                DEFINICJE GLOBALNE (JEDYNE)
 * ========================================================= */
volatile uint8_t enable_zone1 = 0;;
volatile uint8_t enable_zone2 = 0;
volatile uint8_t enable_zone3 = 0;
volatile uint8_t enable_cwu   = 0;
volatile uint8_t enable_room_thermostat_z1;
volatile uint8_t enable_room_thermostat_z2;
volatile uint8_t enable_room_thermostat_z3;
volatile uint8_t enable_circulation = 1;

uint8_t  boiler_cooldown_active = 0;
uint32_t boiler_cooldown_timer  = 0;

uint8_t system_ready = 1;

/* =========================================================
 *                TEMPERATURY (×10)
 * ========================================================= */
int temperature[4];
extern int set_cwu;
extern int set_co1;
extern int set_co2;
extern int set_co3;
uint8_t t_room1;
uint8_t t_room2;
uint8_t t_room3;
uint8_t t_set1;
uint8_t t_set2;
uint8_t t_set3;

/* =========================================================
 *                NRF
 * ========================================================= */
uint8_t tx_data[NRF24L01P_PAYLOAD_LENGTH];
uint8_t rx_data[NRF24L01P_PAYLOAD_LENGTH];

/* =========================================================
 *                ADC FILTER
 * ========================================================= */
#define FILTER_SIZE 50

static uint16_t adc_buf[4][FILTER_SIZE];
static uint32_t adc_sum[4];
static uint8_t  adc_idx[4];
static uint8_t  adc_fill[4];

static uint16_t adc_filter(uint8_t ch, uint16_t v)
{
    adc_sum[ch] -= adc_buf[ch][adc_idx[ch]];
    adc_buf[ch][adc_idx[ch]] = v;
    adc_sum[ch] += v;

    if (++adc_idx[ch] >= FILTER_SIZE) adc_idx[ch] = 0;
    if (adc_fill[ch] < FILTER_SIZE) adc_fill[ch]++;

    return adc_sum[ch] / adc_fill[ch];
}

static void process_adc(void)
{
    for (uint8_t i = 0; i < 4; i++)
        temperature[i] = NTC_ADC2Temperature(adc_filter(i, adc[i]));
}

/* =========================================================
 *                UART – NEXTION
 * ========================================================= */
static void process_uart(void)
{
    static uint8_t cmd[32];
    uint8_t len = uart_cmd_pop(cmd, sizeof(cmd) - 1);
    if (!len) return;

    cmd[len] = 0;

    /* =====================================================
     * KOMENDA: alloffCWP
     * DZIAŁANIE:
     *  - wyłącza wszystkie strefy
     *  - wyłącza CWU
     *  - wyłącza cyrkulację
     *  - uruchamia cooldown kotła
     * ===================================================== */
    if (!strcmp((char*)cmd, "alloffCWP"))
    {
        enable_zone1 = 0;
        enable_zone2 = 0;
        enable_zone3 = 0;
        enable_cwu   = 0;
        enable_circulation = 0;

        boiler_cooldown_active = 1;
        boiler_cooldown_timer  = 0;
        return;
    }

    /* =====================================================
     * KOMENDY CWU
     * ===================================================== */

    /* KOMENDA: cwuonCWP → włączenie CWU */
    if (!strcmp((char*)cmd, "cwuonCWP"))
    {
        enable_cwu = 1;
        return;
    }

    /* KOMENDA: cwuoffCWP → wyłączenie CWU */
    if (!strcmp((char*)cmd, "cwuoffCWP"))
    {
        enable_cwu = 0;

        if((enable_zone1 == 1) || (enable_zone2 == 1) || (enable_zone3 == 1))
        {

       	}
        else
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }

        return;
    }

    /* KOMENDA: cwuxxCWP → ustawienie temperatury CWU
     * PRZYKŁAD: cwu30CWP
     */
    if (!strncmp((char*)cmd, "cwu", 3))
    {
        int temp = atoi((char*)&cmd[3]);
        set_cwu = temp*10;
        return;
    }

    /* =====================================================
     * KOMENDY STREFA 1
     * ===================================================== */

    /* KOMENDA: z1onCWP → włączenie strefy 1 */
    if (!strcmp((char*)cmd, "z1onCWP"))
    {
        enable_zone1 = 1;

        return;
    }
    /* KOMENDA: z1offCWP → wyłączenie strefy 1 */
    if (!strcmp((char*)cmd, "z1offCWP"))
    {
        enable_zone1 = 0;

        if((enable_cwu == 1) || (enable_zone2 == 1) || (enable_zone3 == 1))
        {

       	}
        else
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }
        return;
    }

    /* KOMENDA: z1xxCWP → ustawienie temperatury strefy 1
     * PRZYKŁAD: z125CWP
     */
    if (!strncmp((char*)cmd, "z1", 2))
    {
        int temp = atoi((char*)&cmd[2]);
        set_co1 = temp*10;
        return;
    }

    /* =====================================================
     * KOMENDY STREFA 2
     * ===================================================== */

    /* KOMENDA: z2onCWP → włączenie strefy 2 */
    if (!strcmp((char*)cmd, "z2onCWP"))
    {
        enable_zone2 = 1;
        return;
    }

    /* KOMENDA: z2offCWP → wyłączenie strefy 2 */
    if (!strcmp((char*)cmd, "z2offCWP"))
    {
        enable_zone2 = 0;
        if((enable_zone1 == 1) || (enable_cwu == 1) || (enable_zone3 == 1))
        {

       	}
        else
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }

        return;
    }

    /* KOMENDA: z2xxCWP → ustawienie temperatury strefy 2
     * PRZYKŁAD: z228CWP
     */
    if (!strncmp((char*)cmd, "z2", 2))
    {
        int temp = atoi((char*)&cmd[2]);
        set_co2 = temp*10;
        return;
    }

    /* =====================================================
     * KOMENDY STREFA 3
     * ===================================================== */

    /* KOMENDA: z3onCWP → włączenie strefy 3 */
    if (!strcmp((char*)cmd, "z3onCWP"))
    {
        enable_zone3 = 1;
        return;
    }
    /* KOMENDA: z3offCWP → wyłączenie strefy 3 */
    if (!strcmp((char*)cmd, "z3offCWP"))
    {
        enable_zone3 = 0;
        if((enable_zone1 == 1) || (enable_zone2 == 1) || (enable_cwu == 1))
        {

       	}
        else
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }

        return;
    }
    /* KOMENDA: z3xxCWP → ustawienie temperatury strefy 3
     * PRZYKŁAD: z320CWP
     */
    if (!strncmp((char*)cmd, "z3", 2))
    {
        int temp = atoi((char*)&cmd[2]);
        set_co3 = temp*10;
        return;
    }

    /* =====================================================
     * KOMENDY CYRKULACJA
     * ===================================================== */

    /* KOMENDA: conCWP → włączenie cyrkulacji */
    if (!strcmp((char*)cmd, "conCWP"))
    {
        enable_circulation = 1;
        return;
    }

    /* KOMENDA: coffCWP → wyłączenie cyrkulacji */
    if (!strcmp((char*)cmd, "coffCWP"))
    {
        enable_circulation = 0;
        return;
    }

    /* =====================================================
     * KOMENDY TERMOSTAT POKOJOWY
     * ===================================================== */

    /* KOMENDA: tonCWP → włączenie termostatu pokojowego */
    if (!strcmp((char*)cmd, "t1onCWP"))
    {
    	enable_room_thermostat_z1 = 1;
        return;
    }

    /* KOMENDA: toffCWP → wyłączenie termostatu pokojowego */
    if (!strcmp((char*)cmd, "t1offCWP"))
    {
        enable_room_thermostat_z1 = 0;
        return;
    }

    if (!strcmp((char*)cmd, "t2onCWP"))
    {
    	enable_room_thermostat_z2 = 1;
        return;
    }

    /* KOMENDA: toffCWP → wyłączenie termostatu pokojowego */
    if (!strcmp((char*)cmd, "t2offCWP"))
    {
        enable_room_thermostat_z2 = 0;
        return;
    }

    if (!strcmp((char*)cmd, "t3onCWP"))
    {
    	enable_room_thermostat_z3 = 1;
        return;
    }

    /* KOMENDA: toffCWP → wyłączenie termostatu pokojowego */
    if (!strcmp((char*)cmd, "t3offCWP"))
    {
        enable_room_thermostat_z3 = 0;
        return;
    }
}

/* =========================================================
 *                TIMERY SEKUNDOWE
 * ========================================================= */
static uint32_t last_sec = 0;

static void process_timers(void)
{
    if ((sys_ms - last_sec) >= 1000)
    {
        last_sec = sys_ms;

        if (boiler_cooldown_active)
            boiler_cooldown_timer++;
    }
}

/* =========================================================
 *                NRF FSM (NIEBLOKUJĄCY)
 * ========================================================= */
typedef enum
{
    NRF_IDLE = 0,
    NRF_WAIT_BEFORE_TX,
    NRF_WAIT_AFTER_TX
} nrf_state_t;

static nrf_state_t nrf_state = NRF_IDLE;
static uint32_t nrf_ts = 0;

static void nrf_fsm(void)
{
    switch (nrf_state)
    {
        case NRF_IDLE:
            if (rf_flag)
            {
                rf_flag = 0;
                nrf24l01p_switch_rx_to_tx();
                nrf_ts = sys_ms;
                nrf_state = NRF_WAIT_BEFORE_TX;
            }
            break;

        case NRF_WAIT_BEFORE_TX:
            if ((sys_ms - nrf_ts) >= 1)
            {
                nrf24l01p_tx_transmit(tx_data);
                nrf_ts = sys_ms;
                nrf_state = NRF_WAIT_AFTER_TX;
            }
            break;

        case NRF_WAIT_AFTER_TX:
            if ((sys_ms - nrf_ts) >= 50)
            {
                nrf24l01p_switch_tx_to_rx();
                nrf_state = NRF_IDLE;
            }
            break;
    }
}

/* =========================================================
 *                CYCLE
 * ========================================================= */
void cycle(void)
{
    process_adc();
    process_uart();
    process_timers();

    heat();        /* cała logika grzania */

    nrf_fsm();     /* NRF bez HAL_Delay */

    if (uart_tx_flag)
    {
        SendTemperatureNextion();
        uart_tx_flag = 0;
    }

    if (nrf_rx_flag)
    {
        nrf24l01p_rx_receive(rx_data);
        if (rx_data[0] == 7 && rx_data[1] == 7)
        {

        }
        else {
            t_room1 = rx_data[0];
            t_set1 = rx_data[2];
		}

        nrf_rx_flag = 0;
    }
}
