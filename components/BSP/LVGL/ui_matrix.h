#ifndef UI_MATRIX_H
#define UI_MATRIX_H

#include "lvgl_ui.h"

#define SENSOR_TOTAL_NUM 32
#define CHANNEL_NUM 16
#define UI_MATRIX_16_X_OFFSET 0
#define UI_MATRIX_16_Y_OFFSET 0

#ifdef __cplusplus
extern "C" {
#endif

void ui_matrix_create(void);

void ui_matrix_update(float *data);

#ifdef __cplusplus
}
#endif

#endif
