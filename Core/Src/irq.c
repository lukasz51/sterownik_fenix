#include "irq.h"
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "nrf24l01p.h"
#include "uart_cmd.h"

extern uint8_t  zone_cooldown_active[4];
extern uint32_t zone_cooldown_timer[4];

/* <<< TIM11: flaga 10 s dla cyrkulacji >>> */
volatile uint8_t circulation_tick_10s = 0;

/* =========================================================
 *                SYSTEM TICK (1 ms)
 * ========================================================= */
volatile uint32_t sys_ms = 0;

/* =========================================================
 *                FLAGI
 * ========================================================= */
uint8_t uart_tx_flag = 0;
uint8_t nrf_rx_flag  = 0;
volatile uint8_t rf_flag = 0;

/* =========================================================
 *                UART DMA RX
 * ========================================================= */
#define RX_BUF_SIZE 32
uint8_t rx_buf[RX_BUF_SIZE];

/* =========================================================
 *                GPIO IRQ – NRF
 * ========================================================= */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == NRF24L01P_IRQ_PIN_NUMBER)
    {
        nrf_rx_flag = 1;
    }
}

/* =========================================================
 *                TIMERY
 * ========================================================= */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    /* ---- 1 ms system tick ---- */
    if (htim->Instance == TIM7)
    {
        sys_ms++;
    }

    /* ---- CO cooldown (1 s) ---- */
    if (htim->Instance == TIM6)
    {
        if (zone_cooldown_active[1]) zone_cooldown_timer[1]++;
        if (zone_cooldown_active[2]) zone_cooldown_timer[2]++;
        if (zone_cooldown_active[3]) zone_cooldown_timer[3]++;
    }

    /* <<< TIM11: cyrkulacja tick co 10 s >>> */
    if (htim->Instance == TIM11)
    {
        circulation_tick_10s = 1;
    }

    /* ---- Nextion refresh ---- */
    if (htim->Instance == TIM14)
    {
        uart_tx_flag = 1;
    }

    /* ---- NRF TX trigger ---- */
    if (htim->Instance == TIM13)
    {
        rf_flag = 1;
    }
}

/* =========================================================
 *                UART RX → FIFO
 * ========================================================= */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART1)
    {
        uart_cmd_push(rx_buf, Size);
        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf, RX_BUF_SIZE);
    }
}
