#include "stim.h"

#include "dac80502.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl_ui.h"
#include "stim_adc.h"
#include "ui_matrix.h"

#define HV_CLR_GPIO  GPIO_NUM_40
#define HV_CS_GPIO   GPIO_NUM_39
#define HV_CLK_GPIO  GPIO_NUM_38
#define HV_DIN_GPIO  GPIO_NUM_37
#define HV_DOUT_GPIO GPIO_NUM_36

#define STIM_PRESSURE_MIN 500.0f
#define STIM_PRESSURE_MAX 800.0f
#define STIM_DAC_IDLE_MV 600
#define STIM_DAC_MIN_MV 650
#define STIM_DAC_MAX_MV 1050
#define STIM_PERIOD_MS 40
#define STIM_PULSE_UNIT_US 300
#define STIM_ADC_LOG_PERIOD_MS 500
#define STIM_FORCE_OPEN_CH -1
#define STIM_HV_CH_UNUSED 0xFF

static const char *TAG = "STIM";

static portMUX_TYPE stim_lock = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE stim_hw_lock = portMUX_INITIALIZER_UNLOCKED;
static bool stim_enabled = false;
static uint8_t target_ch = 0;
static uint16_t target_dac_mv = STIM_DAC_IDLE_MV;
static bool target_valid = false;

#if CHANNEL_NUM == 16
static const uint8_t sensor_to_hv_ch[SENSOR_TOTAL_NUM] = {
    7,  STIM_HV_CH_UNUSED, 5,  STIM_HV_CH_UNUSED,
    3,  STIM_HV_CH_UNUSED, 1,  STIM_HV_CH_UNUSED,
    11, STIM_HV_CH_UNUSED, 9,  STIM_HV_CH_UNUSED,
    15, STIM_HV_CH_UNUSED, 13, STIM_HV_CH_UNUSED,
    STIM_HV_CH_UNUSED, 18, STIM_HV_CH_UNUSED, 16,
    STIM_HV_CH_UNUSED, 30, STIM_HV_CH_UNUSED, 28,
    STIM_HV_CH_UNUSED, 26, STIM_HV_CH_UNUSED, 24,
    STIM_HV_CH_UNUSED, 22, STIM_HV_CH_UNUSED, 20,
};
#elif CHANNEL_NUM == 29
static const uint8_t sensor_to_hv_ch[SENSOR_TOTAL_NUM] = {
    7,  6,  5,  4,  3,  STIM_HV_CH_UNUSED, STIM_HV_CH_UNUSED, STIM_HV_CH_UNUSED,
    11, 10, 9,  8,  15, 14, 13, 12,
    19, 18, 17, 16, 31, 30, 29, 28,
    27, 26, 25, 24, 23, 22, 21, 20,
};
#else
#error "Unsupported CHANNEL_NUM"
#endif

static void stim_delay_us(void)
{
    for (uint32_t i = 0; i < 20; i++) {
        asm volatile("nop");
    }
}

static void stim_clear_shift(void)
{
    gpio_set_level(HV_CLR_GPIO, 1);
    stim_delay_us();
    gpio_set_level(HV_CLR_GPIO, 0);
    stim_delay_us();
}

static void stim_clock_pulse(void)
{
    gpio_set_level(HV_CLK_GPIO, 1);
    stim_delay_us();
    gpio_set_level(HV_CLK_GPIO, 0);
    stim_delay_us();
}

static void stim_send_32bit(uint32_t mask)
{
    uint32_t data = mask;

    gpio_set_level(HV_CS_GPIO, 0);
    stim_delay_us();

    for (uint8_t i = 0; i < 32; i++) {
        gpio_set_level(HV_DIN_GPIO, (data & 0x80000000U) ? 1 : 0);
        data <<= 1;
        stim_delay_us();
        stim_clock_pulse();
    }

    stim_delay_us();
    gpio_set_level(HV_CS_GPIO, 1);
}

void stim_init(void)
{
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << HV_CLR_GPIO) |
                        (1ULL << HV_CS_GPIO) |
                        (1ULL << HV_CLK_GPIO) |
                        (1ULL << HV_DIN_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&out_conf));

    gpio_config_t in_conf = {
        .pin_bit_mask = 1ULL << HV_DOUT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&in_conf));

    gpio_set_level(HV_CS_GPIO, 1);
    gpio_set_level(HV_CLR_GPIO, 0);
    gpio_set_level(HV_CLK_GPIO, 0);
    gpio_set_level(HV_DIN_GPIO, 0);

    dac80502_init();
    stim_adc_init();
    stim_close_all();
    ESP_LOGI(TAG, "HV2801 stim initialized");
}

void stim_enable(bool enable)
{
    portENTER_CRITICAL(&stim_lock);
    stim_enabled = enable;
    if (!enable) {
        target_valid = false;
    }
    portEXIT_CRITICAL(&stim_lock);

    if (!enable) {
        dac80502_set_output_mv(STIM_DAC_IDLE_MV, DAC80502_CH_A);
        stim_close_all();
    }

    ESP_LOGI(TAG, "Stim %s", enable ? "enabled" : "disabled");
}

bool stim_is_enabled(void)
{
    bool enabled;

    portENTER_CRITICAL(&stim_lock);
    enabled = stim_enabled;
    portEXIT_CRITICAL(&stim_lock);

    return enabled;
}

void stim_update(const float sensor[32])
{
    float max_val = STIM_PRESSURE_MIN;
    uint8_t max_ch = 0;
    uint16_t dac_mv = STIM_DAC_IDLE_MV;
    bool valid = false;

    for (uint8_t i = 0; i < SENSOR_TOTAL_NUM; i++) {
        if (sensor_to_hv_ch[i] == STIM_HV_CH_UNUSED) {
            continue;
        }

        if (sensor[i] > max_val) {
            max_val = sensor[i];
            max_ch = i;
        }
    }

    if (max_val > STIM_PRESSURE_MIN) {
        float t;

        if (max_val >= STIM_PRESSURE_MAX) {
            t = 1.0f;
        } else {
            t = (max_val - STIM_PRESSURE_MIN) /
                (STIM_PRESSURE_MAX - STIM_PRESSURE_MIN);
        }

        dac_mv = STIM_DAC_MIN_MV +
                 (uint16_t)(t * (float)(STIM_DAC_MAX_MV - STIM_DAC_MIN_MV));
        valid = true;
    }

    portENTER_CRITICAL(&stim_lock);
    target_ch = sensor_to_hv_ch[max_ch];
    target_dac_mv = dac_mv;
    target_valid = valid;
    portEXIT_CRITICAL(&stim_lock);
}

void stim_open_ch(uint8_t ch)
{
    if (ch >= 32) {
        stim_close_all();
        return;
    }

    portENTER_CRITICAL(&stim_hw_lock);
    stim_clear_shift();
    stim_send_32bit(1UL << ch);
    portEXIT_CRITICAL(&stim_hw_lock);
}

void stim_close_all(void)
{
    portENTER_CRITICAL(&stim_hw_lock);
    stim_clear_shift();
    stim_send_32bit(0);
    portEXIT_CRITICAL(&stim_hw_lock);
}

void stim_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();
    TickType_t last_adc_log = 0;
    uint16_t last_dac_mv = 0xFFFF;

#if STIM_FORCE_OPEN_CH >= 0
    ESP_LOGW(TAG, "Test mode: force HV channel %d open", STIM_FORCE_OPEN_CH);
    stim_open_ch(STIM_FORCE_OPEN_CH);
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
#endif

    while (1) {
        bool enabled;
        uint8_t ch;
        uint16_t dac_mv;
        bool valid;
        int adc_raw = stim_adc_read_raw();
        uint8_t sensitive = stim_adc_raw_to_sensitive(adc_raw);
        uint32_t pulse_width_us = sensitive * STIM_PULSE_UNIT_US;
        TickType_t now = xTaskGetTickCount();

        if ((now - last_adc_log) >= pdMS_TO_TICKS(STIM_ADC_LOG_PERIOD_MS)) {
            ESP_LOGI(TAG, "ADC raw: %d, sensitive: %u", adc_raw, sensitive);
            app_lvgl_ui_set_sensitive(sensitive);
            last_adc_log = now;
        }

        portENTER_CRITICAL(&stim_lock);
        enabled = stim_enabled;
        ch = target_ch;
        dac_mv = target_dac_mv;
        valid = target_valid;
        portEXIT_CRITICAL(&stim_lock);

        if (enabled && valid && pulse_width_us > 0) {
            if (dac_mv != last_dac_mv) {
                dac80502_set_output_mv(dac_mv, DAC80502_CH_A);
                last_dac_mv = dac_mv;
            }
            stim_open_ch(ch);
            esp_rom_delay_us(pulse_width_us);
            stim_close_all();
        } else {
            if (last_dac_mv != STIM_DAC_IDLE_MV) {
                dac80502_set_output_mv(STIM_DAC_IDLE_MV, DAC80502_CH_A);
                last_dac_mv = STIM_DAC_IDLE_MV;
            }
            stim_close_all();
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(STIM_PERIOD_MS));
    }
}
