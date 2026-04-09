#include "lcd.h"
#include "esp_check.h"
#include "esp_lcd_st7789.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <stdbool.h>

static const char *TAG = "LCD";

esp_lcd_panel_io_handle_t io_handle = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;

esp_err_t init_lcd_spi() {
    spi_bus_config_t bus_cfg = {
        .sclk_io_num = LCD_CLK,
        .mosi_io_num = LCD_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 0
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    return ESP_OK;
}

esp_err_t init_display() {
    // Initialize backlight
    lcd_backlight_init();
    lcd_backlight_on();

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_DC,
        .cs_gpio_num = LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };

    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                             &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = BITS_PER_PIXEL,
    };

    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(
        esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 0));

    // user can flush pre-defined pattern to the screen before we turn on the
    // screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // this flips the display vertically
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, true));
    
    return ESP_OK;
}

// ===================== Backlight Control Functions =====================

void lcd_backlight_init(void)
{
    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_BLK,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    ESP_LOGI(TAG, "Backlight GPIO initialized on pin %d", LCD_BLK);
}

void lcd_backlight_on(void)
{
    gpio_set_level(LCD_BLK, 1);
    ESP_LOGI(TAG, "Backlight ON");
}

void lcd_backlight_off(void)
{
    gpio_set_level(LCD_BLK, 0);
    ESP_LOGI(TAG, "Backlight OFF");
}

void lcd_backlight_set(bool on)
{
    gpio_set_level(LCD_BLK, on ? 1 : 0);
    ESP_LOGI(TAG, "Backlight set to %s", on ? "ON" : "OFF");
}