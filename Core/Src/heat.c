#include "heat.h"
#include "cycle.h"
#include "relay.h"

/* =========================================================
 *                KONFIGURACJA
 * ========================================================= */
#define BOILER_COOLDOWN_TIME 60   // s
#define ROOM_HYST            1    // 1°C histerezy pokojowej

/* =========================================================
 *                NASTAWY CO (×10)
 * ========================================================= */
int set_co1  = 650, hyst_co1 = 20;
int set_co2  = 600, hyst_co2 = 20;
int set_co3  = 550, hyst_co3 = 20;

/* =========================================================
 *                NASTAWY CWU (×10)
 * ========================================================= */
int set_cwu  = 450, hyst_cwu = 20;

/* =========================================================
 *                BEZPIECZEŃSTWO KOTŁA (×10)
 * ========================================================= */
static int boiler_max  = 700;   // 70°C
static int boiler_hyst = 100;   // 70 → 60

/* =========================================================
 *                STANY
 * ========================================================= */
static uint8_t heating_cwu = 0;
static uint8_t boiler_on  = 0;

/* =========================================================
 *                TRYB KOTŁA (FSM)
 * ========================================================= */
typedef enum {
    BOILER_IDLE = 0,
    BOILER_CWU,
    BOILER_CO1,
    BOILER_CO2,
    BOILER_CO3
} boiler_mode_t;

static boiler_mode_t boiler_mode = BOILER_IDLE;

/* =========================================================
 *                COOLDOWN STREF
 * ========================================================= */
uint8_t  zone_cooldown_active[4] = {0};
uint32_t zone_cooldown_timer[4]  = {0};

/* =========================================================
 *                TERMOSTATY POKOJOWE
 * ========================================================= */
extern volatile uint8_t enable_room_thermostat_z1;
extern volatile uint8_t enable_room_thermostat_z2;
extern volatile uint8_t enable_room_thermostat_z3;

extern uint8_t t_room1, t_room2, t_room3;
extern uint8_t t_set1,  t_set2,  t_set3;

/* =========================================================
 *                TERMOSTAT – LOGIKA
 * ========================================================= */
static uint8_t room_thermostat_allows(uint8_t zone)
{
    static uint8_t allow[4] = {1,1,1,1};
    uint8_t t_room, t_set, enable;

    if (zone == 1) {
        t_room = t_room1; t_set = t_set1; enable = enable_room_thermostat_z1;
    } else if (zone == 2) {
        t_room = t_room2; t_set = t_set2; enable = enable_room_thermostat_z2;
    } else {
        t_room = t_room3; t_set = t_set3; enable = enable_room_thermostat_z3;
    }

    if (!enable || t_set == 0)
        return 1;

    if (allow[zone] && t_room >= t_set)
        allow[zone] = 0;
    else if (!allow[zone] && t_room <= (t_set - ROOM_HYST))
        allow[zone] = 1;

    return allow[zone];
}

/* =========================================================
 *                START COOLDOWNU STREFY
 * ========================================================= */
static void start_zone_cooldown(uint8_t zone)
{
    if (zone < 1 || zone > 3) return;

    zone_cooldown_active[zone] = 1;
    zone_cooldown_timer[zone]  = 0;
}

/* =========================================================
 *                CWU – PRIORYTET
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

    /* STREFA 1 */
    if (zone_cooldown_active[1])
    {
        relay2(1);
        if (zone_cooldown_timer[1] >= BOILER_COOLDOWN_TIME)
        {
            zone_cooldown_active[1] = 0;
            zone_cooldown_timer[1]  = 0;
            relay2(0);
        }
    }
    else
        relay2(enable_zone1 && boiler_on && boiler_mode == BOILER_CO1);

    /* STREFA 2 */
    if (zone_cooldown_active[2])
    {
        relay4(1);
        if (zone_cooldown_timer[2] >= BOILER_COOLDOWN_TIME)
        {
            zone_cooldown_active[2] = 0;
            zone_cooldown_timer[2]  = 0;
            relay4(0);
        }
    }
    else
        relay4(enable_zone2 && boiler_on && boiler_mode == BOILER_CO2);

    /* STREFA 3 */
    if (zone_cooldown_active[3])
    {
        relay5(1);
        if (zone_cooldown_timer[3] >= BOILER_COOLDOWN_TIME)
        {
            zone_cooldown_active[3] = 0;
            zone_cooldown_timer[3]  = 0;
            relay5(0);
        }
    }
    else
        relay5(enable_zone3 && boiler_on && boiler_mode == BOILER_CO3);
}

/* =========================================================
 *                KOCIOŁ – FSM + HISTER.
 * ========================================================= */
static void logic_boiler(void)
{
    /* ===== CWU ===== */
    if (heating_cwu)
    {
        boiler_mode = BOILER_CWU;

        if (boiler_on && temperature[0] >= boiler_max)
            boiler_on = 0;
        else if (!boiler_on &&
                 temperature[0] <= (boiler_max - boiler_hyst))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* ===== STREFA 1 ===== */
    if (enable_zone1 && room_thermostat_allows(1))
    {
        if (boiler_mode != BOILER_CO1)
        {
            boiler_mode = BOILER_CO1;
            boiler_on   = 1;
        }

        if (boiler_on && temperature[0] >= set_co1)
            boiler_on = 0;
        else if (!boiler_on &&
                 temperature[0] <= (set_co1 - hyst_co1))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* ===== STREFA 2 ===== */
    if (enable_zone2 && room_thermostat_allows(2))
    {
        if (boiler_mode != BOILER_CO2)
        {
            boiler_mode = BOILER_CO2;
            boiler_on   = 1;
        }

        if (boiler_on && temperature[0] >= set_co2)
            boiler_on = 0;
        else if (!boiler_on &&
                 temperature[0] <= (set_co2 - hyst_co2))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* ===== STREFA 3 ===== */
    if (enable_zone3 && room_thermostat_allows(3))
    {
        if (boiler_mode != BOILER_CO3)
        {
            boiler_mode = BOILER_CO3;
            boiler_on   = 1;
        }

        if (boiler_on && temperature[0] >= set_co3)
            boiler_on = 0;
        else if (!boiler_on &&
                 temperature[0] <= (set_co3 - hyst_co3))
            boiler_on = 1;

        relay1(boiler_on);
        return;
    }

    /* ===== WYJŚCIE ===== */
    if (boiler_mode != BOILER_IDLE)
    {
        start_zone_cooldown(boiler_mode - BOILER_CO1 + 1);
    }

    boiler_mode = BOILER_IDLE;
    boiler_on   = 0;
    relay1(0);
}

/* =========================================================
 *                API
 * ========================================================= */
void heat(void)
{
    logic_cwu();
    logic_pumps();
    logic_boiler();
}
