#define FILTER_SIZE 50

#include "main.h"
#include "cycle.h"
#include "thermistor.h"
#include <stdint.h>
#include "nextion_com.h"
#include "relay.h"

#include <stdint.h>




// Bufory dla czterech kanałów
static uint16_t adc_buffer[4][FILTER_SIZE];
static uint32_t adc_sum[4] = {0};
static uint8_t adc_index[4] = {0};
static uint8_t adc_filled[4] = {0};

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

    return (uint16_t)(adc_sum[ch] / adc_filled[ch]);
}

// ---- Główna funkcja: filtr + przeliczenie temperatur ----
void process_adc_temperatures(void)
{
    for (uint8_t i = 0; i < 4; i++)
    {
        uint16_t filtered = adc_filter(i, adc[i]);
        temperature[i] = NTC_ADC2Temperature(filtered);
    }
}

void cycle(void)
{
	process_adc_temperatures();
	zone1_control();

}

// zmienne konfiguracyjne
uint8_t enable_zone1 = 1;
int16_t set_temp1 = 450;      // 50.0°C
int16_t hyst1 = 30;           // 5.0°C
int16_t pump_off_temp1 = 300; // 20.0°C (przykład)


// stan grzania
static uint8_t heating_on_1 = 0;
void zone1_control(void)
{
    int16_t temp = temperature[0];

    if (enable_zone1)
    {
        // --- Histereza tylko w dół ---
        if (!heating_on_1 && temp <= (set_temp1 - hyst1)) {
            heating_on_1 = 1;  // włącz grzanie
        }
        else if (heating_on_1 && temp >= set_temp1) {
            heating_on_1 = 0;  // wyłącz grzanie
        }

        // --- Sterowanie kotłem ---
        relay1(heating_on_1);  // 1 = ON, 0 = OFF

        // --- Sterowanie pompą ---
        if (heating_on_1) {
            relay2(1);  // pompa ON przy grzaniu
        }
        else {
            // Pompa wyłącza się tylko, gdy temperatura poniżej progu
            if (temp < pump_off_temp1)
                relay2(0);
            else
                relay2(1);
        }
    }
    else
    {
        // Strefa nieaktywna — wszystko OFF
        relay1(0);
        relay2(0);
        heating_on_1 = 0;
    }
}
