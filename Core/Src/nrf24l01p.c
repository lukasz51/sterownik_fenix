#include "nrf24l01p.h"
#include "delay_us.h"


/* ============================================================
 *  CONFIG
 * ============================================================ */

static const uint8_t NRF_ADDR[5] = {
    0xE7, 0xE7, 0xE7, 0xE7, 0xE7
};



/* ============================================================
 *  GPIO helpers
 * ============================================================ */

static inline void cs_high(void)
{
    HAL_GPIO_WritePin(NRF24L01P_SPI_CS_PIN_PORT,
                      NRF24L01P_SPI_CS_PIN_NUMBER,
                      GPIO_PIN_SET);
}

static inline void cs_low(void)
{
    HAL_GPIO_WritePin(NRF24L01P_SPI_CS_PIN_PORT,
                      NRF24L01P_SPI_CS_PIN_NUMBER,
                      GPIO_PIN_RESET);
}

static inline void ce_high(void)
{
    HAL_GPIO_WritePin(NRF24L01P_CE_PIN_PORT,
                      NRF24L01P_CE_PIN_NUMBER,
                      GPIO_PIN_SET);
}

static inline void ce_low(void)
{
    HAL_GPIO_WritePin(NRF24L01P_CE_PIN_PORT,
                      NRF24L01P_CE_PIN_NUMBER,
                      GPIO_PIN_RESET);
}

/* ============================================================
 *  SPI low level
 * ============================================================ */

static uint8_t read_register(uint8_t reg)
{
    uint8_t cmd = NRF24L01P_CMD_R_REGISTER | reg;
    uint8_t status, val;

    cs_low();
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, &cmd, &status, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(NRF24L01P_SPI, &val, 1, HAL_MAX_DELAY);
    cs_high();

    return val;
}

static void write_register(uint8_t reg, uint8_t value)
{
    uint8_t cmd = NRF24L01P_CMD_W_REGISTER | reg;

    cs_low();
    HAL_SPI_Transmit(NRF24L01P_SPI, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(NRF24L01P_SPI, &value, 1, HAL_MAX_DELAY);
    cs_high();
}

static void write_address(uint8_t reg, const uint8_t *addr, uint8_t len)
{
    uint8_t cmd = NRF24L01P_CMD_W_REGISTER | reg;

    cs_low();
    HAL_SPI_Transmit(NRF24L01P_SPI, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(NRF24L01P_SPI, (uint8_t *)addr, len, HAL_MAX_DELAY);
    cs_high();
}

/* ============================================================
 *  CORE
 * ============================================================ */

void nrf24l01p_reset(void)
{
    cs_high();
    ce_low();

    write_register(NRF24L01P_REG_CONFIG,     0x08);
    write_register(NRF24L01P_REG_EN_AA,      0x3F);   // ACK ON
    write_register(NRF24L01P_REG_EN_RXADDR,  0x01);
    write_register(NRF24L01P_REG_SETUP_AW,   0x03);
    write_register(NRF24L01P_REG_SETUP_RETR, 0x13);   // 500us, 3 retry
    write_register(NRF24L01P_REG_RF_CH,      76);
    write_register(NRF24L01P_REG_RF_SETUP,   0x07);
    write_register(NRF24L01P_REG_STATUS,     0x70);
    write_register(NRF24L01P_REG_DYNPD,      0x00);
    write_register(NRF24L01P_REG_FEATURE,    0x00);

    nrf24l01p_flush_rx_fifo();
    nrf24l01p_flush_tx_fifo();
}

void nrf24l01p_power_up(void)
{
    uint8_t cfg = read_register(NRF24L01P_REG_CONFIG);
    write_register(NRF24L01P_REG_CONFIG, cfg | (1 << 1));
}

void nrf24l01p_prx_mode(void)
{
    uint8_t cfg = read_register(NRF24L01P_REG_CONFIG);
    write_register(NRF24L01P_REG_CONFIG, cfg | (1 << 0));
}

void nrf24l01p_ptx_mode(void)
{
    uint8_t cfg = read_register(NRF24L01P_REG_CONFIG);
    write_register(NRF24L01P_REG_CONFIG, cfg & ~(1 << 0));
}

/* ============================================================
 *  RF
 * ============================================================ */

void nrf24l01p_set_rf_channel(channel MHz)
{
    write_register(NRF24L01P_REG_RF_CH, MHz - 2400);
}

void nrf24l01p_set_rf_air_data_rate(air_data_rate bps)
{
    uint8_t rf = read_register(NRF24L01P_REG_RF_SETUP) & 0xD7;
    if (bps == _2Mbps)      rf |= (1 << 3);
    else if (bps == _250kbps) rf |= (1 << 5);
    write_register(NRF24L01P_REG_RF_SETUP, rf);
}

void nrf24l01p_set_rf_tx_output_power(output_power dBm)
{
    uint8_t rf = read_register(NRF24L01P_REG_RF_SETUP) & 0xF9;
    rf |= (dBm << 1);
    write_register(NRF24L01P_REG_RF_SETUP, rf);
}

/* ============================================================
 *  FIFO
 * ============================================================ */

void nrf24l01p_flush_tx_fifo(void)
{
    uint8_t cmd = NRF24L01P_CMD_FLUSH_TX;
    cs_low();
    HAL_SPI_Transmit(NRF24L01P_SPI, &cmd, 1, HAL_MAX_DELAY);
    cs_high();
}

void nrf24l01p_flush_rx_fifo(void)
{
    uint8_t cmd = NRF24L01P_CMD_FLUSH_RX;
    cs_low();
    HAL_SPI_Transmit(NRF24L01P_SPI, &cmd, 1, HAL_MAX_DELAY);
    cs_high();
}

/* ============================================================
 *  INIT
 * ============================================================ */

void nrf24l01p_rx_init(channel MHz, air_data_rate bps)
{
    nrf24l01p_reset();
    nrf24l01p_prx_mode();
    nrf24l01p_power_up();
    HAL_Delay(2);

    nrf24l01p_set_rf_channel(MHz);
    nrf24l01p_set_rf_air_data_rate(bps);
    nrf24l01p_set_rf_tx_output_power(_0dBm);

    write_address(NRF24L01P_REG_RX_ADDR_P0, NRF_ADDR, 5);
    write_register(NRF24L01P_REG_RX_PW_P0, NRF24L01P_PAYLOAD_LENGTH);

    ce_high();   // RX ciągle włączony
}

void nrf24l01p_tx_init(channel MHz, air_data_rate bps)
{
    nrf24l01p_reset();
    nrf24l01p_ptx_mode();
    nrf24l01p_power_up();
    HAL_Delay(2);

    nrf24l01p_set_rf_channel(MHz);
    nrf24l01p_set_rf_air_data_rate(bps);
    nrf24l01p_set_rf_tx_output_power(_0dBm);

    write_address(NRF24L01P_REG_TX_ADDR,     NRF_ADDR, 5);
    write_address(NRF24L01P_REG_RX_ADDR_P0,  NRF_ADDR, 5);

    ce_low();
}

/* ============================================================
 *  TRANSMIT
 * ============================================================ */

void nrf24l01p_tx_transmit(uint8_t *payload)
{
    // skasuj wszystkie flagi IRQ
    write_register(NRF24L01P_REG_STATUS, 0x70);

    // FIFO musi być puste
    nrf24l01p_flush_tx_fifo();

    // załaduj payload
    uint8_t cmd = NRF24L01P_CMD_W_TX_PAYLOAD;
    cs_low();
    HAL_SPI_Transmit(NRF24L01P_SPI, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Transmit(NRF24L01P_SPI, payload, NRF24L01P_PAYLOAD_LENGTH, HAL_MAX_DELAY);
    cs_high();

    // impuls CE (>=10 µs)
    ce_high();
    delay_us(20);
    ce_low();
}

/* ============================================================
 *  IRQ
 * ============================================================ */

uint8_t nrf24l01p_get_status(void)
{
    uint8_t cmd = NRF24L01P_CMD_NOP;
    uint8_t status;

    cs_low();
    HAL_SPI_TransmitReceive(NRF24L01P_SPI, &cmd, &status, 1, HAL_MAX_DELAY);
    cs_high();

    return status;
}

void nrf24l01p_tx_irq(void)
{
    uint8_t status = nrf24l01p_get_status();

    if (status & (1 << 5))   // TX_DS
        write_register(NRF24L01P_REG_STATUS, (1 << 5));

    if (status & (1 << 4))   // MAX_RT
    {
        nrf24l01p_flush_tx_fifo();
        write_register(NRF24L01P_REG_STATUS, (1 << 4));
    }
}

void nrf24l01p_rx_receive(uint8_t *rx_payload)
{
    uint8_t cmd = NRF24L01P_CMD_R_RX_PAYLOAD;
    cs_low();
    HAL_SPI_Transmit(NRF24L01P_SPI, &cmd, 1, HAL_MAX_DELAY);
    HAL_SPI_Receive(NRF24L01P_SPI, rx_payload, NRF24L01P_PAYLOAD_LENGTH, HAL_MAX_DELAY);
    cs_high();

    write_register(NRF24L01P_REG_STATUS, (1 << 6)); // RX_DR
}

/* ============================================================
 *  MODE SWITCHING RX <-> TX
 * ============================================================ */

/**
 * @brief Przełączenie z RX do TX (bez wysyłania danych)
 *        Zawsze wołaj PRZED nrf24l01p_tx_transmit()
 */
void nrf24l01p_switch_rx_to_tx(void)
{
    // 1️⃣ ZAWSZE CE LOW przed zmianą trybu
    ce_low();

    // 2️⃣ Skasuj wszystkie flagi IRQ
    write_register(NRF24L01P_REG_STATUS, 0x70);

    // 3️⃣ Wyczyść FIFO
    nrf24l01p_flush_rx_fifo();
    nrf24l01p_flush_tx_fifo();

    // 4️⃣ Tryb TX
    nrf24l01p_ptx_mode();

    // 5️⃣ Czas przejścia STBY->TX (datasheet)
    delay_us(130);
}

/**
 * @brief Przełączenie z TX do RX (powrót do nasłuchu)
 *        Wołaj PO wysłaniu danych
 */
void nrf24l01p_switch_tx_to_rx(void)
{
    // 1️⃣ CE LOW
    ce_low();

    // 2️⃣ Skasuj IRQ
    write_register(NRF24L01P_REG_STATUS, 0x70);

    // 3️⃣ Wyczyść FIFO
    nrf24l01p_flush_tx_fifo();
    nrf24l01p_flush_rx_fifo();

    // 4️⃣ Tryb RX
    nrf24l01p_prx_mode();

    // 5️⃣ Czas STBY->RX
    delay_us(130);

    // 6️⃣ Start nasłuchu
    ce_high();
}

