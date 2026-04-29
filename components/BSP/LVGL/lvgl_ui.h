#ifndef __LVGL_UI_H__
#define __LVGL_UI_H__

#define LCD_H_RES 240
#define LCD_V_RES 280
#define LCD_DRAW_BUFF_HEIGHT 70

#define LCD_DRAW_BUFF_DOUBLE 1

// 左右手模式选择：注释掉下面这行使用左手，取消注释使用右手
#define HAND_RIGHT

void app_lvgl_ui_init(void);

#endif
