#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "lvgl_ui.h"   // ⭐ 你的UI封装头文件
#include "uart_receive.h"

static const char *TAG = "MAIN";

static void send_start_cmd()
{
    uint8_t cmd_start[18] = {0};

    cmd_start[0] = 0xFF;
    cmd_start[1] = 0xFF;
    cmd_start[2] = 0x06;
    cmd_start[3] = 0x09;

    cmd_start[17] = 0x11; // ⭐关键

    uart_send_data(cmd_start, 18);

    printf("Start CMD sent\r\n");
}

void app_main(void)
{
    ESP_LOGI(TAG, "System start");

    /* ⭐ 初始化 LVGL + LCD + UI（全部在 lvgl_ui.c 里完成） */
    app_lvgl_ui_init();

    ESP_LOGI(TAG, "UI init done");

    usart_init(2000000);
    vTaskDelay(pdMS_TO_TICKS(1000)); // 等系统稳定

    send_start_cmd();  // ⭐发送启动命令

    xTaskCreate(uart_receive_task,
                "uart_rx",
                8192,
                NULL,
                10,
                NULL);

}
