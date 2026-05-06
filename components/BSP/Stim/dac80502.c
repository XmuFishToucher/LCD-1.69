#include "dac80502.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DAC80502_CLK_GPIO  GPIO_NUM_47
#define DAC80502_DATA_GPIO GPIO_NUM_48
#define DAC80502_SYNC_GPIO GPIO_NUM_26

#define DAC80502_REG_GAIN  0x04
#define DAC80502_REG_DAC_A 0x08
#define DAC80502_REG_DAC_B 0x09

#define DAC80502_REF_MV 2500

static const char *TAG = "DAC80502";
static portMUX_TYPE dac_lock = portMUX_INITIALIZER_UNLOCKED;

static inline void dac_delay(void)
{
    for (uint32_t i = 0; i < 20; i++) {
        asm volatile("nop");
    }
}

static void dac_tx_byte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++) {
        gpio_set_level(DAC80502_DATA_GPIO, (data & 0x80) ? 1 : 0);
        data <<= 1;
        dac_delay();

        gpio_set_level(DAC80502_CLK_GPIO, 1);
        dac_delay();
        gpio_set_level(DAC80502_CLK_GPIO, 0);
        dac_delay();
    }
}

static void dac_write_reg(uint8_t reg, uint16_t value)
{
    portENTER_CRITICAL(&dac_lock);

    gpio_set_level(DAC80502_SYNC_GPIO, 0);
    dac_delay();
    dac_tx_byte(reg & 0x0F);
    dac_tx_byte((value >> 8) & 0xFF);
    dac_tx_byte(value & 0xFF);
    dac_delay();
    gpio_set_level(DAC80502_SYNC_GPIO, 1);

    portEXIT_CRITICAL(&dac_lock);
}

static uint16_t dac_mv_to_code(uint16_t mv)
{
    if (mv > DAC80502_REF_MV) {
        mv = DAC80502_REF_MV;
    }

    return (uint16_t)(((uint32_t)mv * 65535U) / DAC80502_REF_MV);
}

void dac80502_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DAC80502_CLK_GPIO) |
                        (1ULL << DAC80502_DATA_GPIO) |
                        (1ULL << DAC80502_SYNC_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    gpio_set_level(DAC80502_SYNC_GPIO, 1);
    gpio_set_level(DAC80502_CLK_GPIO, 0);
    gpio_set_level(DAC80502_DATA_GPIO, 0);

    vTaskDelay(pdMS_TO_TICKS(2));

    dac_write_reg(DAC80502_REG_GAIN, 0x0103);
    dac80502_set_output_mv(600, DAC80502_CH_A);
    dac80502_set_output_mv(52, DAC80502_CH_B);

    ESP_LOGI(TAG, "DAC80502 initialized");
}

void dac80502_set_output_mv(uint16_t mv, uint8_t channel)
{
    uint8_t reg;

    if (channel == DAC80502_CH_A) {
        reg = DAC80502_REG_DAC_A;
    } else if (channel == DAC80502_CH_B) {
        reg = DAC80502_REG_DAC_B;
    } else {
        return;
    }

    dac_write_reg(reg, dac_mv_to_code(mv));
}
