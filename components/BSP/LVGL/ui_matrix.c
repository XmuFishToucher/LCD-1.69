#include "lvgl.h"
#include <stdio.h>

#define ROW 8
#define COL 4
#define POINT_NUM 32

static lv_obj_t *cells[POINT_NUM];

static lv_color_t heatmap_color(uint8_t intensity)
{
    float t = intensity / 255.0f;

    uint8_t r, g, b;

    if (t < 0.25f) {
        // 蓝 → 青
        r = 0;
        g = (uint8_t)(t / 0.25f * 255);
        b = 255;
    } else if (t < 0.5f) {
        // 青 → 绿
        r = 0;
        g = 255;
        b = (uint8_t)((1.0f - (t - 0.25f) / 0.25f) * 255);
    } else if (t < 0.75f) {
        // 绿 → 黄
        r = (uint8_t)((t - 0.5f) / 0.25f * 255);
        g = 255;
        b = 0;
    } else {
        // 黄 → 红
        r = 255;
        g = (uint8_t)((1.0f - (t - 0.75f) / 0.25f) * 255);
        b = 0;
    }

    return lv_color_make(r, g, b);
}

void ui_matrix_create(void)
{
    int screen_w = 240;
    int screen_h = 280;

    int dot_diameter = 16;
    int dot_spacing = 4;

    // 计算网格尺寸
    int grid_w = COL * dot_diameter + (COL - 1) * dot_spacing;
    int grid_h = ROW * dot_diameter + (ROW - 1) * dot_spacing;

    int start_x = (screen_w - grid_w) / 2;
    int start_y = (screen_h - grid_h) / 2;

    lv_obj_t *screen = lv_scr_act();

    for (int i = 0; i < POINT_NUM; i++) {
        int r = i / COL;
        int c = i % COL;

        lv_obj_t *obj = lv_obj_create(screen);

        int pos_x = start_x + c * (dot_diameter + dot_spacing);
        int pos_y = start_y + r * (dot_diameter + dot_spacing);

        lv_obj_set_size(obj, dot_diameter, dot_diameter);
        lv_obj_set_pos(obj, pos_x, pos_y);

        lv_obj_set_style_radius(obj, dot_diameter / 2, 0);
        lv_obj_set_style_bg_color(obj, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_100, 0);
        lv_obj_set_style_border_width(obj, 0, 0);
        lv_obj_set_style_shadow_width(obj, 0, 0);
        lv_obj_set_style_outline_width(obj, 0, 0);

        lv_obj_move_foreground(obj);

        cells[i] = obj;
    }

    printf("Matrix UI created: %d points, grid %dx%d at (%d,%d)\r\n",
           POINT_NUM, grid_w, grid_h, start_x, start_y);
}

#define VALUE_MIN  500.0f
#define VALUE_MAX  800.0f

void ui_matrix_update(float *data)
{
    for (int i = 0; i < POINT_NUM; i++) {
        float val = data[i];

        uint8_t intensity;
        if (val < VALUE_MIN)
            intensity = 0;
        else if (val > VALUE_MAX)
            intensity = 255;
        else
            intensity = (uint8_t)((val - VALUE_MIN) / (VALUE_MAX - VALUE_MIN) * 255.0f);

        lv_color_t color = heatmap_color(intensity);
        lv_obj_set_style_bg_color(cells[i], color, 0);
    }

}
