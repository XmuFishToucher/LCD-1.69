#ifndef DAC80502_H
#define DAC80502_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DAC80502_CH_A 1
#define DAC80502_CH_B 2

void dac80502_init(void);
void dac80502_set_output_mv(uint16_t mv, uint8_t channel);

#ifdef __cplusplus
}
#endif

#endif
