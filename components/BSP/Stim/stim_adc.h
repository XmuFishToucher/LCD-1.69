#ifndef STIM_ADC_H
#define STIM_ADC_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void stim_adc_init(void);
uint8_t stim_adc_get_sensitive(void);
uint8_t stim_adc_raw_to_sensitive(int raw);
int stim_adc_read_raw(void);

#ifdef __cplusplus
}
#endif

#endif
