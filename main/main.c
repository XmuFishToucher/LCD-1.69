#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"

#include "lvgl_ui.h"   // ⭐ 你的UI封装头文件

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "System start");

    /* ⭐ 初始化 LVGL + LCD + UI（全部在 lvgl_ui.c 里完成） */
    app_lvgl_ui_init();

    ESP_LOGI(TAG, "UI init done");

}