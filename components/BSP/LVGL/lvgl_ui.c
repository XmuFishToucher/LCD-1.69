#include "lvgl_ui.h"
#include "lcd.h"
#include "esp_lvgl_port.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_check.h"
#include "ui_matrix.h"
#include "uart_receive.h"
#include "driver/i2c_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"

static const char *TAG = "LVGL_UI";

static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;
static esp_lcd_touch_handle_t touch_handle = NULL;

#ifdef HAND_RIGHT
extern const lv_image_dsc_t hand_map_right;
#else
extern const lv_image_dsc_t hand_map;
#endif

static esp_err_t app_touch_init(void)
{
    i2c_master_bus_handle_t i2c_handle = NULL;
    const i2c_master_bus_config_t i2c_config = {
        .i2c_port = 0,
        .sda_io_num = LCD_TP_SDA,
        .scl_io_num = LCD_TP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_config, &i2c_handle), TAG, "I2C init failed");

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = LCD_TP_TINT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();

    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(i2c_handle, &tp_io_config, &tp_io_handle), TAG, "Touch IO init failed");

    return esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &touch_handle);
}

static esp_err_t app_lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,         /* LVGL task priority */
        .task_stack = 8192,         /* LVGL task stack size */
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

    /* Add touch input */
    ESP_RETURN_ON_ERROR(app_touch_init(), TAG, "Touch init failed");
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}

static void zero_btn_cb(lv_event_t *e)
{
    uart_zero_calibrate();
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
#ifdef HAND_RIGHT
    lv_image_set_src(img, &hand_map_right);
#else
    lv_image_set_src(img, &hand_map);
#endif

    /* 设置位置 */
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    /* 创建阵点可视化层（覆盖在手图之上） */
    ui_matrix_create();

    /* 创建调零按钮 */
    lv_obj_t *btn = lv_button_create(scr);
    lv_obj_set_size(btn, 70, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, -10, -30);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, "ZERO");
    lv_obj_center(label);
    lv_obj_add_event_cb(btn, zero_btn_cb, LV_EVENT_CLICKED, NULL);

    /* 解锁 */
    lvgl_port_unlock();
}