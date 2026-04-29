#ifndef UI_MATRIX_H
#define UI_MATRIX_H

#include "lvgl_ui.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_matrix_create(void);

void ui_matrix_update(float *data);

#ifdef __cplusplus
}
#endif

#endif
