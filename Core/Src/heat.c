#include "heat.h"
#include "cycle.h"
#include "relay.h"
/* =========================================================
 *                KONFIGURACJA
 * ========================================================= */
#define BOILER_COOLDOWN_TIME 60   // s

/* ===== nastawy CO (×10) ===== */
int set_co1 = 650, hyst_co1 = 20;
int set_co2 = 600, hyst_co2 = 20;
int set_co3 = 550, hyst_co3 = 20;

/* ===== nastawy CWU (×10) ===== */
int set_cwu = 450, hyst_cwu = 20;
/* ===== bezpieczeństwo kotła (×10) ===== */
static int boiler_max = 700;   // 70.0°C
static int boiler_hyst = 100;  // 70 → 60

/* =========================================================
 *                CYRKULACJA CWU
 * ========================================================= */


/* czasy w sekundach */
static uint32_t circulation_period  = 1200; // 20 min
static uint32_t circulation_on_time = 120;  // 2 min

static uint32_t circulation_timer   = 0;
static uint32_t circulation_runtime = 0;
static uint8_t  circulation_active  = 0;

/* =========================================================
 *                STANY
 * ========================================================= */
static uint8_t heating_cwu = 0;
static uint8_t boiler_on  = 0;

/* =========================================================
 *                CWU (PRIORYTET)
 * ========================================================= */
static void logic_cwu(void)
{
    if (!enable_cwu)
    {
        heating_cwu = 0;
        relay3(0);
        return;
    }

    if (!heating_cwu && temperature[1] <= (set_cwu - hyst_cwu))
        heating_cwu = 1;
    else if (heating_cwu && temperature[1] >= set_cwu)
        heating_cwu = 0;

    relay3(heating_cwu);
}

/* =========================================================
 *                CYRKULACJA – NIEZALEŻNA OD TEMP
 * ========================================================= */
static void logic_circulation(void)
{
    if (!enable_circulation)
    {
        relay6(0);
        circulation_active  = 0;
        circulation_timer   = 0;
        circulation_runtime = 0;
        return;
    }

    if (!circulation_active)
    {
        if (circulation_timer >= circulation_period)
        {
            circulation_active  = 1;
            circulation_runtime = 0;
            relay6(1);
        }
    }
    else
    {
        if (circulation_runtime >= circulation_on_time)
        {
            circulation_active = 0;
            circulation_timer  = 0;
            relay6(0);
        }
    }
}

/* =========================================================
 *                POMPY CO + COOLDOWN
 * ========================================================= */
static void logic_pumps(void)
{
    if (heating_cwu)
    {
        relay2(0);
        relay4(0);
        relay5(0);
        return;
    }

    if (boiler_cooldown_active)
    {
        relay2(1);
        relay4(0);
        relay5(0);

        if (boiler_cooldown_timer >= BOILER_COOLDOWN_TIME)
        {
            boiler_cooldown_active = 0;
            boiler_cooldown_timer  = 0;
            relay2(0);
        }
        return;
    }

    relay2(enable_zone1);
    relay4(enable_zone2);
    relay5(enable_zone3);
}

/* =========================================================
 *                KOCIOŁ (HISTEREZA + PRIORYTET)
 * ========================================================= */
static void logic_boiler(void)
{
    /* =========================
     * TRYB CWU – PRIORYTET
     * ========================= */
    if (heating_cwu)
    {
        /* wyłącz przy 70°C */
        if (boiler_on && temperature[0] >= boiler_max)
            boiler_on = 0;

        /* włącz dopiero przy 60°C */
        else if (!boiler_on &&
                 temperature[0] <= (boiler_max - boiler_hyst))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* =========================
     * TRYB CO – STREFY
     * ========================= */

    /* strefa 1 */
    if (enable_zone1)
    {
        if (boiler_on && temperature[0] >= set_co1)
            boiler_on = 0;
        else if (!boiler_on &&
                 temperature[0] <= (set_co1 - hyst_co1))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* strefa 2 */
    if (enable_zone2)
    {
        if (boiler_on && temperature[0] >= set_co2)
            boiler_on = 0;
        else if (!boiler_on &&
                 temperature[0] <= (set_co2 - hyst_co2))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* strefa 3 */
    if (enable_zone3)
    {
        if (boiler_on && temperature[0] >= set_co3)
            boiler_on = 0;
        else if (!boiler_on &&
                 temperature[0] <= (set_co3 - hyst_co3))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* =========================
     * BRAK ZAPOTRZEBOWANIA
     * ========================= */
    boiler_on = 0;
    relay1(0);
}


/* =========================================================
 *                API
 * ========================================================= */
void heat(void)
{
    logic_cwu();
    logic_circulation();
    logic_pumps();
    logic_boiler();
}
