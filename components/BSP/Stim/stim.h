#ifndef STIM_H
#define STIM_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void stim_init(void);
void stim_task(void *arg);
void stim_enable(bool enable);
bool stim_is_enabled(void);
void stim_update(const float sensor[32]);
void stim_open_ch(uint8_t ch);
void stim_close_all(void);

#ifdef __cplusplus
}
#endif

#endif
