#ifndef __LCD_H__
#define __LCD_H__
#include <stdbool.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#define LCD_HOST SPI2_HOST

#define LCD_CLK 13
#define LCD_MOSI 14
#define PIN_NUM_MISO -1
#define LCD_DC 11
#define LCD_CS 12
#define LCD_RST 15

#define LCD_TP_RST 15
#define LCD_TP_TINT 16
#define LCD_TP_SDA 17
#define LCD_TP_SCL 18

#define LCD_BLK 10  // Backlight control pin

#define LCD_PIXEL_CLOCK_HZ (60 * 1000 * 1000)

#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8

#define BITS_PER_PIXEL 16

esp_err_t init_lcd_spi(void);

esp_err_t init_display(void);

extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_panel_handle_t panel_handle;

// Backlight control functions
void lcd_backlight_init(void);
void lcd_backlight_on(void);
void lcd_backlight_off(void);
void lcd_backlight_set(bool on);

#endif