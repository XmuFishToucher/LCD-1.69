#include "lvgl_ui.h"
#include "lcd.h"
#include "esp_lvgl_port.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_check.h"

static const char *TAG = "LVGL_UI";

static lv_display_t *lvgl_disp = NULL;

extern const lv_image_dsc_t hand_map;

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,         /* LVGL task priority */
        .task_stack = 4096,         /* LVGL task stack size */
        .task_affinity = -1,        /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500,   /* Maximum sleep in LVGL task */
        .timer_period_ms = 5        /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_DRAW_BUFF_HEIGHT,
        .double_buffer = LCD_DRAW_BUFF_DOUBLE,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
#if LVGL_VERSION_MAJOR >= 9
        .color_format = LV_COLOR_FORMAT_RGB565,
#endif
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
#if LVGL_VERSION_MAJOR >= 9
            .swap_bytes = true,
#endif
        }
    };
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    return ESP_OK;
}

void app_lvgl_ui_init(void)
{
    ESP_ERROR_CHECK(init_lcd_spi());
    ESP_ERROR_CHECK(init_display());
    ESP_ERROR_CHECK(app_lvgl_init());

    /* ⭐ LVGL 操作必须加锁 */
    lvgl_port_lock(0);

    /* 获取当前屏幕 */
    lv_obj_t *scr = lv_scr_act();

    /* 创建图片对象 */
    lv_obj_t *img = lv_image_create(scr);

    /* 设置图片源 */
    lv_image_set_src(img, &hand_map);

    /* 设置位置 */
    lv_obj_align(img, LV_ALIGN_CENTER, 5, 15);  // 往下15像素

    /* 解锁 */
    lvgl_port_unlock();
}