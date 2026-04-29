#include "lvgl.h"
#include <stdio.h>

#define POINT_NUM 29

// 阵点网格坐标 (gx, gy) 及对应数据通道号
// gx: -1~4, gy: -3~3, cell = 20px (16px圆点 + 4px间距)
// 通道 20,21,22 未使用
typedef struct {
    uint8_t ch;   // data[] 中的索引
    float gx;     // 网格 x（支持小数，eg. 0.5 = 半格）
    float gy;     // 网格 y
} point_def_t;

static const point_def_t points[POINT_NUM] = {

    // ============================================================
    //  手掌 (4×4 方形网格) — 不动
    // ============================================================
    // gy=0: 手掌顶行
    { 2,  0,  0},  {15,  1,  0},  {19,  2,  0},  {28,  3,  0},
    // gy=1
    { 3,  0,  1},  { 8,  1,  1},  {18,  2,  1},  {27,  3,  1},
    // gy=2
    { 4,  0,  2},  { 9,  1,  2},  {17,  2,  2},  {26,  3,  2},
    // gy=3: 手掌底行
    { 5,  0,  3},  {10,  1,  3},  {16,  2,  3},  {25,  3,  3},

    // ============================================================
    //  拇指 (左侧)
    // ============================================================
    { 7, -3.3f,  0},
    { 6, -2.2f,  1},

    // ============================================================
    //  食指 (指尖→指根, 连接 ch2)
    // ============================================================
    { 1,  -1, -4.7f},
    { 0,  -0.8f, -3.15f},
    {11,  -0.6f, -1.9f},

    // ============================================================
    //  中指 (指尖→指根, 连接 ch15)
    // ============================================================
    {14,  1.35f, -5.55f},
    {13,  1.15f, -3.85f},
    {12,  1.1f, -2.45f},

    // ============================================================
    //  无名指 (指尖→指根, 连接 ch28)
    // ============================================================
    {31,  2.95f, -4.7f},
    {30,  2.65f, -3.2f},
    {29,  2.5f, -2},

    // ============================================================
    //  小指侧 (右下)
    // ============================================================
    {23,  4.8f,  -3},
    {24,  4.4f,  -1.5f},
};

static lv_obj_t *cells[POINT_NUM];

static lv_color_t heatmap_color(uint8_t intensity)
{
    float t = intensity / 255.0f;

    uint8_t r, g, b;

    if (t < 0.25f) {
        r = 0;
        g = (uint8_t)(t / 0.25f * 255);
        b = 255;
    } else if (t < 0.5f) {
        r = 0;
        g = 255;
        b = (uint8_t)((1.0f - (t - 0.25f) / 0.25f) * 255);
    } else if (t < 0.75f) {
        r = (uint8_t)((t - 0.5f) / 0.25f * 255);
        g = 255;
        b = 0;
    } else {
        r = 255;
        g = (uint8_t)((1.0f - (t - 0.75f) / 0.25f) * 255);
        b = 0;
    }

    return lv_color_make(r, g, b);
}

void ui_matrix_create(void)
{
    int dot_diameter = 16;
    int dot_spacing = 4;
    int cell = dot_diameter + dot_spacing; // 20px

    // 网格范围: gx=-1..4 (6列), gy=-3..3 (7行)
    int cols = 6;
    int rows = 7;
    int grid_w = cols * cell - dot_spacing; // 6*20-4 = 116
    int grid_h = rows * cell - dot_spacing; // 7*20-4 = 136

    int origin_x = (240 - grid_w) / 2 + 20; // +20 = 右移 1 个 cell
    int origin_y = (280 - grid_h) / 2;
    int min_gx = -1;
    int min_gy = -3;

    lv_obj_t *screen = lv_scr_act();

    for (int i = 0; i < POINT_NUM; i++) {
        lv_obj_t *obj = lv_obj_create(screen);

        int pos_x = origin_x + (int)((points[i].gx - min_gx) * cell);
        int pos_y = origin_y + (int)((points[i].gy - min_gy) * cell);

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
           POINT_NUM, grid_w, grid_h, origin_x, origin_y);
}

#define VALUE_MIN  5.0f
#define VALUE_MAX  50.0f

void ui_matrix_update(float *data)
{
    for (int i = 0; i < POINT_NUM; i++) {
        float val = data[points[i].ch];

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
