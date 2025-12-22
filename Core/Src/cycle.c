#define FILTER_SIZE 50

#include "main.h"
#include "cycle.h"
#include "thermistor.h"
#include <stdint.h>
#include "nextion_com.h"
#include "relay.h"
#include "nrf24l01p.h"
#include "math.h"

uint8_t tx_data[NRF24L01P_PAYLOAD_LENGTH] = {0, 0, 0, 0, 0, 0, 0, 0};
extern uint8_t uart_tx_flag;
uint8_t rx_data[NRF24L01P_PAYLOAD_LENGTH];
extern uint8_t nrf_rx_flag;
// Bufory filtrów
static uint16_t adc_buffer[4][FILTER_SIZE];
static uint32_t adc_sum[4] = {0};
static uint8_t adc_index[4] = {0};
static uint8_t adc_filled[4] = {0};
extern volatile uint8_t rf_flag;
uint32_t adc[8];
int temperature[4];

// ---- Filtr średniej kroczącej ----
static uint16_t adc_filter(uint8_t ch, uint16_t new_sample)
{
    adc_sum[ch] -= adc_buffer[ch][adc_index[ch]];
    adc_buffer[ch][adc_index[ch]] = new_sample;
    adc_sum[ch] += new_sample;

    adc_index[ch]++;
    if (adc_index[ch] >= FILTER_SIZE)
        adc_index[ch] = 0;

    if (adc_filled[ch] < FILTER_SIZE)
        adc_filled[ch]++;

    return adc_sum[ch] / adc_filled[ch];
}

// ---- Przeliczanie temperatur ----
void process_adc_temperatures(void)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        uint16_t filtered = adc_filter(i, adc[i]);
        temperature[i] = NTC_ADC2Temperature(filtered);
    }
}

// -------------------------
//      ZMIENNE CO
// -------------------------
volatile uint8_t enable_zone1 = 0;
volatile uint8_t enable_zone2 = 0;
volatile uint8_t enable_zone3 = 0;
int16_t set_temp1 = 350;      // 45.0°C
int16_t hyst1 = 20;           // 3.0°C
int16_t pump_off_temp1 = 300; // 30.0°C

static uint8_t heating_on_1 = 0;

// -------------------------
//      ZMIENNE CWU
// -------------------------
volatile uint8_t enable_cwu = 0;        // 1 = grzej CWU
int16_t set_cwu = 450;         // temp CWU (45.0°C)
int16_t hyst_cwu = 20;         // histereza CWU (2°C)
static uint8_t heating_cwu = 0;

// -------------------------
//   OGRANICZENIE 70°C
// -------------------------
#define MAX_BOILER_TEMP 700    // 70.0°C
#define MAX_BOILER_HYST 50     // 5°C
static uint8_t boiler_cooldown = 0;  // blokada powrotu na CO/CWU OFF

// -------------------------
//     STEROWANIE CWU
// -------------------------
void cwu_control(void)
{
    int16_t boiler = temperature[0]; // czujnik za kotłem
    int16_t cwu = temperature[1];    // czujnik bojlera

    // --- kontrola przegrzania kotła ---
    if (boiler > MAX_BOILER_TEMP)
        boiler_cooldown = 1;      // wymuszone chłodzenie
    else if (boiler < (MAX_BOILER_TEMP - MAX_BOILER_HYST))
        boiler_cooldown = 0;


    // --- logika grzania CWU ---
    if (enable_cwu)
    {
        // start CWU
        if (!heating_cwu && cwu <= (set_cwu - hyst_cwu))
            heating_cwu = 1;

        // stop CWU – tylko jeśli NIE ma przegrzania kotła
        if (heating_cwu && !boiler_cooldown && cwu >= set_cwu)
            heating_cwu = 0;
    }
    else
    {
        heating_cwu = 0;
    }


    // --- pompa CWU (relay3) działa gdy CWU grzeje LUB chłodzimy kocioł ---
    if (heating_cwu || boiler_cooldown)
        relay3(1);
    else
        relay3(0);


    // --- sterowanie kotłem podczas CWU i chłodzenia ---
    if (heating_cwu)
    {
        if (!boiler_cooldown)
            relay1(1);       // normalne grzanie CWU
        else
            relay1(0);       // przerwa kotła – ale pompa CWU nadal pracuje
    }
    else if (boiler_cooldown)
    {
        // CWU dogrzane, ale temperatura kotła za wysoka
        relay1(0);
        relay3(1);           // pompa CWU chłodzi
    }
}


// -------------------------
//        STEROWANIE CO
// -------------------------
void zone1_control(void)
{
    int16_t temp = temperature[0];

    // CO wyłączone gdy CWU aktywne lub chłodzenie kotła
    if (heating_cwu || boiler_cooldown)
    {
        relay2(0);     // pompa CO OFF
        heating_on_1 = 0;
        return;        // kocioł sterowany przez CWU
    }

    // Normalna logika CO
    if (enable_zone1)
    {
        if (!heating_on_1 && temp <= (set_temp1 - hyst1))
            heating_on_1 = 1;

        else if (heating_on_1 && temp >= set_temp1)
            heating_on_1 = 0;

        relay1(heating_on_1);

        if (heating_on_1)
            relay2(1);
        else
        {
            if (temp < pump_off_temp1)
                relay2(0);
            else
                relay2(1);
        }
    }
    else
    {
        relay1(0);
        relay2(0);
        heating_on_1 = 0;
    }
}


// -------------------------
//       CYKL GŁÓWNY
// -------------------------
void cycle(void)
{
    process_adc_temperatures();
    cwu_control();
    zone1_control();


	if (rf_flag == 1)
	{

		nrf24l01p_switch_rx_to_tx();
		tx_data[0] = round(temperature[0] / 10); // temp co
		tx_data[1] = round(temperature[1] / 10); // temp cwu
		tx_data[2] = enable_cwu;
		tx_data[3] = enable_zone1;
		tx_data[4] = enable_zone2;
		tx_data[5] = enable_zone3;
		tx_data[6] = heating_on_1;
		tx_data[7] = heating_cwu;

		HAL_Delay(1);
		nrf24l01p_tx_transmit(tx_data);
		HAL_Delay(50);
		nrf24l01p_switch_tx_to_rx();
		rf_flag = 0;
	}

	if(uart_tx_flag == 1)
	{
		  SendTemperatureNextion();
		  HAL_GPIO_TogglePin(MCU_LED_GPIO_Port, MCU_LED_Pin);
		  uart_tx_flag = 0;
	}

	if(nrf_rx_flag==1)
	{
		nrf24l01p_rx_receive(rx_data);
		nrf_rx_flag = 0;
	}


}

