#include <stdio.h>
#include <string.h>
#include "uart.h"
#include "ui_matrix.h"
#include "esp_lvgl_port.h"

#define FRAME_SIZE 212
#define UART_BUF_MAX 1024

static uint8_t uart_buf[UART_BUF_MAX];
static int uart_len = 0;

static float sensor_value[32];

// =========================
// 解析UDP_11
// =========================
static void parse_udp11(uint8_t *buf)
{
    float max_val = 0;
    int max_idx = 0;

    for (int i = 0; i < 32; i++)
    {
        int offset = 20 + i * 6;

        int int_part =
            (buf[offset] << 16) |
            (buf[offset + 1] << 8) |
            buf[offset + 2];

        int dec_part =
            (buf[offset + 3] << 16) |
            (buf[offset + 4] << 8) |
            buf[offset + 5];

        sensor_value[i] =
            int_part + dec_part / 1000000.0f;

        // 找最大值（后面做电刺激直接用）
        if (sensor_value[i] > max_val)
        {
            max_val = sensor_value[i];
            max_idx = i;
        }
    }

    // ===== 打印 =====
    printf("Frame OK | Max: %.3f @ %d\r\n", max_val, max_idx);

    for (int i = 0; i < 32; i++)
    {
        printf("%.2f ", sensor_value[i]);
        if ((i + 1) % 8 == 0)
            printf("\r\n");
    }

    printf("------------------------\r\n");

    // 更新阵点可视化
    lvgl_port_lock(0);
    ui_matrix_update(sensor_value);
    lvgl_port_unlock();
}


// =========================
// 查找帧头（提高鲁棒性）
// =========================
static int find_frame_head(uint8_t *buf, int len)
{
    for (int i = 0; i < len - 1; i++)
    {
        if (buf[i] == 0xFF && buf[i + 1] == 0xFF)
        {
            return i;
        }
    }
    return -1;
}


// =========================
// 解析缓冲区
// =========================
static void parse_uart_buffer()
{
    while (uart_len >= FRAME_SIZE)
    {
        int head = find_frame_head(uart_buf, uart_len);

        if (head < 0)
        {
            uart_len = 0; // 清空
            return;
        }

        // 移动到帧头
        if (head > 0)
        {
            memmove(uart_buf, uart_buf + head, uart_len - head);
            uart_len -= head;
        }

        // 不够一帧
        if (uart_len < FRAME_SIZE)
            return;

        // 判断是不是UDP_11
        if (uart_buf[16] == 0x00 && uart_buf[17] == 0x12)
        {
            parse_udp11(uart_buf);

            // 移除一帧
            memmove(uart_buf, uart_buf + FRAME_SIZE, uart_len - FRAME_SIZE);
            uart_len -= FRAME_SIZE;
        }
        else
        {
            // 错帧，丢1字节重新找
            memmove(uart_buf, uart_buf + 1, uart_len - 1);
            uart_len -= 1;
        }
    }
}


// =========================
// UART任务
// =========================
void uart_receive_task(void *arg)
{
    uint8_t temp_buf[256];

    while (1)
    {
        int len = uart_recv_data(temp_buf, sizeof(temp_buf), 20);
        
        if (len > 0)
        {
            // 防溢出
            if (uart_len + len > UART_BUF_MAX)
            {
                uart_len = 0;
            }

            memcpy(uart_buf + uart_len, temp_buf, len);
            uart_len += len;

            parse_uart_buffer();
        }
    }
}