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
volatile uint8_t enable_zone1 = 0;
volatile uint8_t enable_zone2 = 0;
volatile uint8_t enable_zone3 = 0;
volatile uint8_t enable_cwu   = 0;
volatile uint8_t enable_room_thermostat_z1;
volatile uint8_t enable_room_thermostat_z2;
volatile uint8_t enable_room_thermostat_z3;
volatile uint8_t enable_circulation = 0;
extern uint32_t circulation_on_time;   // w tickach 10 s
extern uint32_t circulation_off_time;  // w tickach 10 s
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
#define NRF_TH_COUNT 3

uint8_t tx_data[NRF24L01P_PAYLOAD_LENGTH];
uint8_t rx_data[NRF24L01P_PAYLOAD_LENGTH];

static const uint8_t NRF_ADDR_LIST[NRF_TH_COUNT][5] =
{
    {0xE7,0xE7,0xE7,0xE7,0xE7}, // termostat 1
    {0xE7,0xE7,0xE7,0xE7,0x02}, // termostat 2
    {0xE7,0xE7,0xE7,0xE7,0x03}  // termostat 3
};

static uint8_t current_thermostat = 0;   // DO KOGO TX
static uint8_t rx_thermostat      = 0;   // OD KOGO RX

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
     * ALL OFF
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
     * CWU
     * ===================================================== */
    if (!strcmp((char*)cmd, "cwuonCWP"))
    {
        enable_cwu = 1;
        return;
    }

    if (!strcmp((char*)cmd, "cwuoffCWP"))
    {
        enable_cwu = 0;
        if (!(enable_zone1 || enable_zone2 || enable_zone3))
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }
        return;
    }

    if (!strncmp((char*)cmd, "cwu", 3))
    {
        set_cwu = atoi((char*)&cmd[3]) * 10;
        return;
    }

    /* =====================================================
     * STREFA 1
     * ===================================================== */
    if (!strcmp((char*)cmd, "z1onCWP"))
    {
        enable_zone1 = 1;
        return;
    }

    if (!strcmp((char*)cmd, "z1offCWP"))
    {
        enable_zone1 = 0;
        if (!(enable_cwu || enable_zone2 || enable_zone3))
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }
        return;
    }

    if (!strncmp((char*)cmd, "z1", 2))
    {
        set_co1 = atoi((char*)&cmd[2]) * 10;
        return;
    }

    /* =====================================================
     * STREFA 2
     * ===================================================== */
    if (!strcmp((char*)cmd, "z2onCWP"))
    {
        enable_zone2 = 1;
        return;
    }

    if (!strcmp((char*)cmd, "z2offCWP"))
    {
        enable_zone2 = 0;
        if (!(enable_zone1 || enable_cwu || enable_zone3))
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }
        return;
    }

    if (!strncmp((char*)cmd, "z2", 2))
    {
        set_co2 = atoi((char*)&cmd[2]) * 10;
        return;
    }

    /* =====================================================
     * STREFA 3
     * ===================================================== */
    if (!strcmp((char*)cmd, "z3onCWP"))
    {
        enable_zone3 = 1;
        return;
    }

    if (!strcmp((char*)cmd, "z3offCWP"))
    {
        enable_zone3 = 0;
        if (!(enable_zone1 || enable_zone2 || enable_cwu))
        {
            boiler_cooldown_active = 1;
            boiler_cooldown_timer  = 0;
        }
        return;
    }

    if (!strncmp((char*)cmd, "z3", 2))
    {
        set_co3 = atoi((char*)&cmd[2]) * 10;
        return;
    }

    /* =====================================================
     * CYRKULACJA – ON / OFF
     * ===================================================== */
    if (!strcmp((char*)cmd, "coonCWP"))
    {
        enable_circulation = 1;
        return;
    }

    if (!strcmp((char*)cmd, "cooffCWP"))
    {
        enable_circulation = 0;
        return;
    }

    /* =====================================================
     * CYRKULACJA – CZASY
     * ===================================================== */

    /* cot1CWP → czas pracy (minuty) */
    if (!strncmp((char*)cmd, "cot1CWP", 7))
    {
        int minutes = atoi((char*)&cmd[7]);
        if (minutes > 0)
            circulation_on_time = minutes * 6;   // 1 min = 6 × 10 s
        return;
    }

    /* cot2CWP → czas przerwy (minuty) */
    if (!strncmp((char*)cmd, "cot2CWP", 7))
    {
        int minutes = atoi((char*)&cmd[7]);
        if (minutes > 0)
            circulation_off_time = minutes * 6;  // 1 min = 6 × 10 s
        return;
    }

    /* =====================================================
     * TERMOSTATY POKOJOWE
     * ===================================================== */
    if (!strcmp((char*)cmd, "t1onCWP"))  { enable_room_thermostat_z1 = 1; return; }
    if (!strcmp((char*)cmd, "t1offCWP")) { enable_room_thermostat_z1 = 0; return; }

    if (!strcmp((char*)cmd, "t2onCWP"))  { enable_room_thermostat_z2 = 1; return; }
    if (!strcmp((char*)cmd, "t2offCWP")) { enable_room_thermostat_z2 = 0; return; }

    if (!strcmp((char*)cmd, "t3onCWP"))  { enable_room_thermostat_z3 = 1; return; }
    if (!strcmp((char*)cmd, "t3offCWP")) { enable_room_thermostat_z3 = 0; return; }
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

                /* ustaw adres DO TX */
                nrf24l01p_set_address(NRF_ADDR_LIST[current_thermostat]);

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

                /* zapamiętaj, od kogo oczekujemy RX */
                rx_thermostat = current_thermostat;

                /* przygotuj indeks NA NASTĘPNY CYKL */
                current_thermostat++;
                if (current_thermostat >= NRF_TH_COUNT)
                    current_thermostat = 0;

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

    heat();

    nrf_fsm();

    if (uart_tx_flag)
    {
        SendTemperatureNextion();
        uart_tx_flag = 0;
    }

    if (nrf_rx_flag)
    {
        nrf24l01p_rx_receive(rx_data);

        if (!(rx_data[0] == 7 && rx_data[1] == 7))
        {
            switch (rx_thermostat)
            {
                case 0: t_room1 = rx_data[0]; t_set1 = rx_data[2]; break;
                case 1: t_room2 = rx_data[0]; t_set2 = rx_data[2]; break;
                case 2: t_room3 = rx_data[0]; t_set3 = rx_data[2]; break;
            }
        }

        nrf_rx_flag = 0;
    }
}
