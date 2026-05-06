#include "stim_adc.h"

#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_log.h"

#define STIM_ADC_UNIT ADC_UNIT_1
#define STIM_ADC_CH   ADC_CHANNEL_2
#define STIM_ADC_ATTEN ADC_ATTEN_DB_12
#define STIM_ADC_SENSITIVE_MAX 10

static const char *TAG = "STIM_ADC";
static adc_oneshot_unit_handle_t adc_handle = NULL;

void stim_adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = STIM_ADC_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = STIM_ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, STIM_ADC_CH, &chan_cfg));

    ESP_LOGI(TAG, "Pot ADC initialized on GPIO3");
}

int stim_adc_read_raw(void)
{
    int raw = 0;

    if (adc_handle == NULL) {
        return 0;
    }

    if (adc_oneshot_read(adc_handle, STIM_ADC_CH, &raw) != ESP_OK) {
        return 0;
    }

    return raw;
}

uint8_t stim_adc_get_sensitive(void)
{
    int raw = stim_adc_read_raw();

    if (raw < 0) {
        raw = 0;
    } else if (raw > 4095) {
        raw = 4095;
    }

    return (uint8_t)((raw * STIM_ADC_SENSITIVE_MAX) / 4095);
}
