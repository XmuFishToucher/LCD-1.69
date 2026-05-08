# LCD-1.69 项目文档

更新时间：2026-05-08

## 项目概述

本项目是基于 ESP-IDF 5.3.3 和 ESP32-S3 的触觉显示与电刺激反馈工程。

当前主要功能：

- 通过 UART2 接收外部 32 路压力传感器数据。
- 使用 LVGL 在 1.69 英寸 ST7789 LCD 上显示右手压力热力图。
- 提供触摸 UI 控件：`ZERO` 调零、`STIM` 电刺激开关、`SEN` 灵敏度显示。
- 使用 HV2801 选择电刺激通道。
- 使用 DAC80502 的 DACA 控制电流镜幅值。
- 使用 DAC80502 的 DACB 作为固定比较阈值，目前为 `52 mV`。
- 使用 GPIO3 ADC 电位器控制电刺激脉宽，也就是占空比。

当前有效基线是右手模式：

```c
#define HAND_RIGHT
```

左手分支暂时不维护，后续如需使用需要重新校准。

## 关键文件

```text
main/main.c
components/BSP/CMakeLists.txt
components/BSP/UART/uart_receive.c
components/BSP/LVGL/lvgl_ui.c
components/BSP/LVGL/lvgl_ui.h
components/BSP/LVGL/ui_matrix.c
components/BSP/LVGL/hand_map.c
components/BSP/LVGL/hand_map_right.c
components/BSP/Stim/stim.c
components/BSP/Stim/stim.h
components/BSP/Stim/dac80502.c
components/BSP/Stim/dac80502.h
components/BSP/Stim/stim_adc.c
components/BSP/Stim/stim_adc.h
```

## 硬件连接

### LCD：ST7789

| 信号 | GPIO | 说明 |
|---|---:|---|
| CLK | GPIO13 | SPI 时钟 |
| MOSI | GPIO14 | SPI 数据 |
| DC | GPIO11 | 数据/命令选择 |
| CS | GPIO12 | 片选 |
| RST | GPIO15 | 复位 |
| BLK | GPIO10 | 背光 |

### 触摸：CST816S

| 信号 | GPIO |
|---|---:|
| SDA | GPIO17 |
| SCL | GPIO18 |
| INT | GPIO16 |

### UART2 传感器输入

| 信号 | GPIO | 说明 |
|---|---:|---|
| TX | GPIO34 | UART2 TX |
| RX | GPIO35 | UART2 RX |

UART 参数：

```text
2,000,000 bps，8N1，无流控
```

### HV2801 通道选择

| 信号 | GPIO | 说明 |
|---|---:|---|
| HV_CLR | GPIO40 | MTDO |
| HV_CS | GPIO39 | MTCK |
| HV_CLK | GPIO38 | 移位时钟 |
| HV_DIN | GPIO37 | 移位数据输入 |
| HV_DOUT | GPIO36 | 输入，仅配置，当前未使用 |

HV2801 使用完整 32 位 mask：

```c
stim_send_32bit(1UL << hv_ch);
```

### DAC80502

| 信号 | 原理图网络 | GPIO |
|---|---|---:|
| SCK | SPICLK_P | GPIO47 |
| MOSI | SPICLK_N | GPIO48 |
| CS/SYNC | SPICS1 | GPIO26 |

DAC80502 使用 GPIO 模拟 24 bit SPI。

当前初始化：

```c
dac_write_reg(DAC80502_REG_GAIN, 0x0103);
dac80502_set_output_mv(600, DAC80502_CH_A);
dac80502_set_output_mv(52, DAC80502_CH_B);
```

含义：

- DACA：电流镜控制电压，影响电刺激电流幅值。
- DACB：比较器固定参考阈值，参考旧项目保持为 `52 mV`。

### 电位器 ADC

| 信号 | GPIO | ADC |
|---|---:|---|
| 电位器输出 | GPIO3 | ADC_UNIT_1 / ADC_CHANNEL_2 |

ADC raw `0~4095` 映射为 `sensitive 0~10`。

## 启动流程

```text
app_main()
  -> stim_init()
       -> 初始化 HV2801 GPIO
       -> dac80502_init()
       -> stim_adc_init()
  -> app_lvgl_ui_init()
       -> 初始化 LCD
       -> 初始化 LVGL
       -> 显示手部位图
       -> ui_matrix_create()
       -> 创建 ZERO 按钮
       -> 创建 STIM 开关
       -> 创建 SEN 标签
  -> usart_init(2000000)
  -> xTaskCreate(stim_task)
  -> xTaskCreate(uart_receive_task)
  -> send_start_cmd()
```

## 传感数据流程

UART 解析后保存 32 路传感器数据。当前数据流程为：

```c
raw_value[i] = int_part + dec_part / 1000000.0f;
zeroed_value = raw_value[i] - zero_offset[i];
filtered_value[i] += SENSOR_FILTER_ALPHA * (zeroed_value - filtered_value[i]);
sensor_value[i] = filtered_value[i];
```

当前滤波参数在 `uart_receive.c`：

```c
#define SENSOR_FILTER_ALPHA 0.2f
```

显示和电刺激都使用调零并滤波后的 `sensor_value`：

```c
ui_matrix_update(sensor_value);
stim_update(sensor_value);
```

点击 `ZERO` 后不再只取单帧，而是累计多帧 raw 值取平均作为零点：

```c
#define ZERO_CALIBRATE_FRAMES 10
zero_offset[i] = zero_sum[i] / ZERO_CALIBRATE_FRAMES;
```

多帧清零期间，当前实现会继续进入 UART 数据流程；如果佩戴贴片后基线仍然快速漂移，可能出现清零后很快重新变红的现象。

当前已关闭逐帧串口打印：

```c
#define UART_PRINT_SENSOR_FRAME 0
```

## UI 显示

当前屏幕元素：

- 右手位图：`hand_map_right`
- 压力热力点数量由 `ui_matrix.h` 中的 `CHANNEL_NUM` 决定，当前为 `16`
- 右下角 `ZERO` 按钮
- 左下角 `STIM` 开关
- `STIM` 上方显示 `SEN:x`

热力图阈值在 `ui_matrix.c`：

```c
#define VALUE_MIN  500.0f
#define VALUE_MAX  800.0f
```

显示逻辑基于调零后的压力变化量：

```text
sensor < 500       -> 不明显变色
sensor 500~800     -> 逐渐变红
sensor > 800       -> 最红
```

## 右手显示映射

当前右手显示映射定义在 `ui_matrix.c`：

```c
static const point_def_t points[POINT_NUM]
```

显示点数量配置在 `ui_matrix.h`：

```c
#define SENSOR_TOTAL_NUM 32
#define CHANNEL_NUM 16
#define UI_MATRIX_16_X_OFFSET 0
#define UI_MATRIX_16_Y_OFFSET 0
```

`CHANNEL_NUM == 16` 时只显示当前使用的手掌 4x4 点位，UART 仍然接收 32 路数据。当前 16 点显示布局：

```text
第 0 行：ch0  ch12 ch19 ch27
第 1 行：ch2  ch14 ch17 ch25
第 2 行：ch4  ch8  ch31 ch23
第 3 行：ch6  ch10 ch29 ch21
```

16 点模式下，几何位置对齐到原 29 点右手布局中的手掌区域。若实机显示整体偏移，可只调整：

```c
#define UI_MATRIX_16_X_OFFSET 0
#define UI_MATRIX_16_Y_OFFSET 0
```

`CHANNEL_NUM == 29` 时保留完整右手布局：

```text
手掌第 0 行：ch25 ch16 ch12 ch11
手掌第 1 行：ch24 ch31 ch13 ch0
手掌第 2 行：ch23 ch30 ch14 ch1
手掌第 3 行：ch22 ch29 ch15 ch2

大拇指：ch20 ch21
食指：  ch26 ch27 ch28
中指：  ch17 ch18 ch19
无名指：ch8  ch9  ch10
小拇指：ch4  ch3
```

完整 29 点右手布局中，传感器通道 `5、6、7` 未使用；当前 16 点模式中，传感器通道 `6` 已启用。

## 电刺激逻辑

电刺激参数在 `stim.c`：

```c
#define STIM_PRESSURE_MIN 500.0f
#define STIM_PRESSURE_MAX 800.0f
#define STIM_DAC_IDLE_MV 600
#define STIM_DAC_MIN_MV 650
#define STIM_DAC_MAX_MV 1050
#define STIM_PERIOD_MS 40
#define STIM_PULSE_UNIT_US 300
#define STIM_ADC_LOG_PERIOD_MS 500
#define STIM_FORCE_OPEN_CH -1
```

压力到 DACA 的映射：

```text
sensor <= 500      -> 无有效刺激，DACA 回到 600 mV
500 < sensor < 800 -> DACA 线性映射到 650~1050 mV
sensor >= 800      -> DACA = 1050 mV
```

刺激周期和脉宽：

```text
周期 = 40 ms，约 25 Hz
sensitive = 0~10
pulse_width_us = sensitive * 300 us
```

当前脉宽/占空比范围：

```text
0~3000 us
25 Hz 下约 0~7.5%
```

`stim_task()` 每 `500 ms` 打印一次 ADC raw 和 sensitive，并同步更新屏幕上的 `SEN:x`。

## 当前任务优先级

当前代码已恢复到调整优先级前的配置：

```text
LVGL task priority = 4
stim_task priority = 6
uart_rx priority = 10
```

曾尝试将 LVGL 提高到 8、UART 降到 7 来改善触摸响应，但对“清零后阵点重新变红”的现象帮助不明显，因此已回退。当前判断该现象主要来自贴片佩戴后的基线漂移、机械接触波动或传感噪声，而不是单纯的触摸任务优先级问题。

## 传感通道到 HV 通道映射

电刺激不直接使用 UI 坐标，而是：

```text
找到最大压力传感通道 -> 查 sensor_to_hv_ch[] -> 打开对应 HV 通道
```

当前 `CHANNEL_NUM == 16` 映射表：

```c
#define STIM_HV_CH_UNUSED 0xFF

static const uint8_t sensor_to_hv_ch[SENSOR_TOTAL_NUM] = {
    7,  STIM_HV_CH_UNUSED, 5,  STIM_HV_CH_UNUSED,
    3,  STIM_HV_CH_UNUSED, 1,  STIM_HV_CH_UNUSED,
    11, STIM_HV_CH_UNUSED, 9,  STIM_HV_CH_UNUSED,
    15, STIM_HV_CH_UNUSED, 13, STIM_HV_CH_UNUSED,
    STIM_HV_CH_UNUSED, 18, STIM_HV_CH_UNUSED, 16,
    STIM_HV_CH_UNUSED, 30, STIM_HV_CH_UNUSED, 28,
    STIM_HV_CH_UNUSED, 26, STIM_HV_CH_UNUSED, 24,
    STIM_HV_CH_UNUSED, 22, STIM_HV_CH_UNUSED, 20,
};
```

当前 16 点明确对应关系：

```text
sensor 0  -> HV7
sensor 12 -> HV15
sensor 19 -> HV16
sensor 27 -> HV24

sensor 2  -> HV5
sensor 14 -> HV13
sensor 17 -> HV18
sensor 25 -> HV26

sensor 4  -> HV3
sensor 8  -> HV11
sensor 31 -> HV20
sensor 23 -> HV28

sensor 6  -> HV1
sensor 10 -> HV9
sensor 29 -> HV22
sensor 21 -> HV30
```

注意：当前规则按“传感通道号固定对应硬件 HV 通道”处理。例如显示点从传感 `16` 改为传感 `12` 后，应使用 `sensor 12 -> HV15`，而不是继承原显示位置上 `sensor 16 -> HV19`。

`CHANNEL_NUM == 29` 时完整对应关系：

```text
小拇指：
sensor 4  -> HV3
sensor 3  -> HV4

无名指：
sensor 8  -> HV11
sensor 9  -> HV10
sensor 10 -> HV9

中指：
sensor 17 -> HV18
sensor 18 -> HV17
sensor 19 -> HV16

食指：
sensor 26 -> HV25
sensor 27 -> HV24
sensor 28 -> HV23

大拇指：
sensor 20 -> HV31
sensor 21 -> HV30

手掌：
sensor 25 -> HV26
sensor 24 -> HV27
sensor 23 -> HV28
sensor 22 -> HV29
sensor 16 -> HV19
sensor 31 -> HV20
sensor 30 -> HV21
sensor 29 -> HV22
sensor 12 -> HV15
sensor 13 -> HV14
sensor 14 -> HV13
sensor 15 -> HV12
sensor 11 -> HV8
sensor 0  -> HV7
sensor 1  -> HV6
sensor 2  -> HV5
```

未使用通道会在 `stim_update()` 中跳过，因此这些通道的噪声不会触发电刺激。

## HV 固定通道测试模式

调试 HV2801 时可以使用固定通道模式：

```c
#define STIM_FORCE_OPEN_CH 12
```

这样会固定打开一个 HV 通道，并跳过正常的脉冲刺激逻辑。

正常使用必须恢复为：

```c
#define STIM_FORCE_OPEN_CH -1
```

当前文档记录的状态是正常模式。

## 已知调试记录

- DACA 已确认可以随压力强度变化，说明 DAC 通信和压力映射基本正常。
- `V_135_B` 曾测得约 `40 V`，说明高压源可能未达到预期的 135 V。
- 脉冲模式下用万用表测 HV 输出可能不准确，建议用示波器或高压探头。
- HV2801 调试建议使用固定通道模式。
- `hand_map.c` 曾出现空 `#if` 和 `LV_ATTRIBUTE_IMAGE_HAND_MAP` 未定义问题，已按 `hand_map_right.c` 模板修复。

## 调试过程记录

### 2026-04-29：UART 缓冲区溢出导致帧错位

现象：

- 传感器数据偶尔出现异常大值。
- 部分数据帧后半段解析异常。

处理：

- UART 缓冲区溢出时不再直接清空全部数据。
- 改为丢弃最老数据，并尝试重新搜索帧头 `0xFF 0xFF`。
- 提高了长时间运行时的帧同步稳定性。

### 2026-04-29：LVGL 跨任务操作风险

现象：

- UART 接收任务会直接刷新 UI 阵点。
- 存在与 LVGL task 并发访问 UI 对象的风险。

处理：

- 在 `uart_receive.c` 刷新热力点前增加 `lvgl_port_lock(pdMS_TO_TICKS(20))`。
- 如果拿不到锁，则跳过本次 UI 刷新，避免阻塞 UART 接收。

### 2026-04-29：上电后偶尔收不到有效数据

现象：

- 上电后偶尔没有有效传感器帧。
- 需要重新启动或重新发送启动命令。

原因分析：

- UART 初始化早于接收任务，RX 上电噪声可能残留。
- 启动命令可能在接收任务准备完成前发出。
- 缺少超时重发机制。

处理：

- 接收任务创建后延时 `100 ms` 再发送启动命令。
- `uart_receive_task()` 启动时调用 `uart_flush_input()` 清空 UART 驱动缓冲。
- 超过 `3 s` 未收到有效帧时自动重发启动命令。

### 2026-04-29：阵点布局重构

处理：

- 将原来的简单矩形网格改为手部形状布局。
- `point_def_t` 中的 `gx/gy` 改为 `float`，支持半格和更细的位置调整。
- 右手映射逐步校准为当前有效版本。

### 2026-04-29：增加触摸调零

处理：

- 增加 CST816S 触摸输入注册。
- 新增右下角 `ZERO` 按钮。
- 新增 `raw_value[32]`、`zero_offset[32]` 和 `uart_zero_calibrate()`。
- 点击 `ZERO` 后使用当前原始值作为 32 路零点。

### 2026-04-29：ZERO 点击后重启

现象：

- 点击 `ZERO` 后出现重启或异常。

原因：

- LVGL task 栈空间不足。

处理：

- 将 LVGL task 栈从较小值扩大到 `8192` 字节。

### 2026-05-06：集成电刺激基础模块

处理：

- 新增 `components/BSP/Stim/` 模块。
- 增加 HV2801 GPIO 控制。
- 增加 DAC80502 GPIO 模拟 SPI 驱动。
- 增加 GPIO3 ADC 电位器读取。
- 在 `main.c` 中调用 `stim_init()` 并创建 `stim_task`。
- 在 `uart_receive.c` 中将调零后的 `sensor_value[32]` 传入 `stim_update()`。
- 在 UI 中增加 `STIM` 开关，默认关闭。

### 2026-05-06：BSP CMake 依赖修正

现象：

- ESP-IDF CMake 配置阶段报组件依赖解析错误。

处理：

- `components/BSP/CMakeLists.txt` 增加 Stim 源文件和 include 路径。
- 依赖中使用 `log`，不是 `esp_log`。
- 增加 `driver`、`esp_driver_gpio`、`esp_adc`、`esp_rom` 等依赖。

### 2026-05-06：误动 ESP-IDF 框架文件

现象：

- VS Code 打开的 ESP-IDF 示例脚本显示被修改。

处理：

- 在 ESP-IDF 仓库中恢复 `examples/build_system/cmake/idf_as_lib` 目录。
- 本项目后续只修改 `D:\Documents\ESP_Projects\LCD-1.69` 工作区内文件。

### 2026-05-06：串口输出过多

现象：

- 串口持续输出 32 路阵点数据，影响调试其他日志。

处理：

- 在 `uart_receive.c` 中加入：

```c
#define UART_PRINT_SENSOR_FRAME 0
```

- 逐帧 `Frame OK` 和 32 路数值打印默认关闭。

### 2026-05-06：ADC 和 sensitive 调试

处理：

- 新增 `stim_adc_raw_to_sensitive()`，确保 ADC raw 和 sensitive 来自同一次采样。
- `stim_task()` 每 `500 ms` 打印：

```text
ADC raw: x, sensitive: y
```

- UI 左下角增加 `SEN:x` 标签，并移动到 `STIM` 标签上方。

### 2026-05-06：刺激阈值确认

讨论与结论：

- 之前电刺激使用的是调零后的 `sensor_value`。
- 曾短暂尝试根据 raw 零点动态设置 `STIM_PRESSURE_MIN`。
- 后续确认保持原逻辑：基于调零后的压力变化量，`500~800` 控制刺激幅值。

当前状态：

```text
调零后压力 > 500 开始刺激
调零后压力 >= 800 达到最大 DAC 幅值
```

### 2026-05-06：热力图阈值与刺激阈值对齐

处理：

- `ui_matrix.c` 的显示阈值调整为：

```c
#define VALUE_MIN  500.0f
#define VALUE_MAX  800.0f
```

结果：

- 屏幕变色范围与电刺激触发范围一致。

### 2026-05-06：DACB 作用确认

过程：

- 阅读旧项目 `E:\Document\Code\H723_ALL_IN_ONE`。
- 重点查看 `dac80502_gpio.c`、`tazer.c`、`current_control.c`。
- 对照当前电流镜原理图。

结论：

- DACA 是运行时电流幅值控制端。
- DACB 只在初始化时设置为 `52 mV`。
- 原理图中 DACB 与 `NEG1` 比较，用于比较器阈值和 LED/CTL 指示，不参与刺激强度动态控制。

### 2026-05-06：电流输出问题排查

现象：

- DACA 能随强度变化。
- 但测试时没有明显电流输出。

排查结论：

- DAC 通信和压力到电压映射基本正常。
- 需要优先确认高压源、HV2801 通道、负载回路和测量方式。
- `V_135_B` 实测约 `40 V`，低于预期 135 V，说明高压源可能未正常升压或被负载拉低。

建议：

- 关闭全部通道时测 `V_135_B`。
- 固定打开单通道时再测 `V_135_B` 和对应 HV 输出。
- 使用示波器或高压探头，不建议只用万用表判断短脉冲输出。

### 2026-05-06：HV 固定通道测试

处理：

- 增加 `STIM_FORCE_OPEN_CH` 测试宏。
- 设置为非负数时，`stim_task()` 固定打开指定 HV 通道并停止正常脉冲逻辑。
- 测试过固定打开 `29` 号通道和 `12` 号通道。

当前正常模式：

```c
#define STIM_FORCE_OPEN_CH -1
```

### 2026-05-06：占空比范围增大

处理：

- 将单档脉宽从 `100 us` 逐步调大，当前为：

```c
#define STIM_PULSE_UNIT_US 300
```

当前范围：

```text
sensitive = 0  -> 0 us
sensitive = 10 -> 3000 us
25 Hz 下占空比约 0~7.5%
```

### 2026-05-06：电刺激通道映射完成

处理：

- 新增 `sensor_to_hv_ch[32]`。
- 显示映射仍由 `ui_matrix.c` 的 `points[]` 决定。
- 电刺激通道改为“传感通道 -> HV 通道”查表。
- 未使用通道 `5、6、7` 标记为 `STIM_HV_CH_UNUSED`，并在 `stim_update()` 中跳过。

结果：

- 当前右手传感点位和 HV 电刺激点位已按实测关系分离映射。

### 2026-05-07：16 点手掌模式与几何位置调整

处理：

- 在 `ui_matrix.h` 中增加 `CHANNEL_NUM`，当前设置为 `16`。
- UART 接收仍保持 32 路，不修改协议和原始数据数组。
- `CHANNEL_NUM == 16` 时，UI 只显示当前使用的 4x4 手掌点位。
- `stim.c` 中的 `sensor_to_hv_ch[]` 也按 `CHANNEL_NUM` 分支，未使用传感通道标记为 `STIM_HV_CH_UNUSED`。
- 传感通道 `6` 已加入当前 16 点模式，并映射到 `HV1`。
- 将 16 点阵列几何位置对齐到原 29 点右手布局的手掌区域。
- 增加 `UI_MATRIX_16_X_OFFSET` 和 `UI_MATRIX_16_Y_OFFSET`，用于实机微调阵点整体偏移。

关键确认：

- 传感通道和 HV 的关系按硬件通道号固定对应，不按显示点位继承。
- 因此当前显示使用 `sensor 12` 时，应映射到原 `sensor 12` 对应的 `HV15`，不是原显示位置上 `sensor 16` 的 `HV19`。

### 2026-05-08：贴片佩戴后波动与清零策略尝试

现象：

- 贴片带在手上后，传感信号波动明显增大。
- 点击 `ZERO` 后阵点会先变淡，随后又快速变红，清零效果不稳定。

已尝试处理：

- 在 UART 数据路径中增加 EMA 一阶低通滤波：

```c
#define SENSOR_FILTER_ALPHA 0.2f
```

- 将单帧清零改为 10 帧 raw 平均清零：

```c
#define ZERO_CALIBRATE_FRAMES 10
```

- 曾尝试提高 LVGL 任务优先级、降低 UART 接收任务优先级，以改善触摸响应；效果不明显，已恢复为原优先级配置。

当前判断：

- 清零按键本身不是主要问题。
- 更可能是贴片佩戴后的机械应力变化、接触状态变化或基线漂移持续存在，导致清零完成后新的调零数据仍然很快超过显示/刺激阈值。

后续建议：

- 增加清零后死区，例如小于某个阈值的变化直接置 0。
- 对负值进行归零或单独处理，避免回弹和漂移影响最大值选择。
- 进一步增加清零帧数，如 `30` 或 `50` 帧。
- 将显示阈值和刺激阈值分开：显示可以敏感，刺激阈值应更保守。
- 机械上固定贴片和线束，减少手部动作带来的传感点应力变化。

### 2026-05-06：hand_map.c 编译错误修复

现象：

```text
hand_map.c:20:4: error: #if with no expression
unknown type name 'LV_ATTRIBUTE_IMAGE_HAND_MAP'
```

原因：

- `hand_map.c` 头部生成模板异常，出现空的 `#if`。
- `LV_ATTRIBUTE_IMAGE_HAND_MAP` 默认宏未定义。

处理：

- 按 `hand_map_right.c` 模板修复为：

```c
#ifndef LV_ATTRIBUTE_IMAGE_HAND_MAP
#define LV_ATTRIBUTE_IMAGE_HAND_MAP
#endif
```

## 构建命令

在 ESP-IDF 5.3.3 环境中执行：

```powershell
idf.py build
idf.py flash monitor
```

如果普通 PowerShell 找不到 `idf.py`，需要先打开 ESP-IDF 终端，或执行 Espressif 环境初始化脚本。

## 测试与安全注意

- 不要直接从人体测试开始。
- 先用假负载验证 DAC 输出、HV2801 通道、刺激电流路径。
- 早期测试建议降低 `STIM_DAC_MAX_MV`。
- 打开 `STIM` 前先把电位器调到最小脉宽。
- 正常使用前确认 `STIM_FORCE_OPEN_CH` 为 `-1`。
- 如果再次测试固定通道，测试完成后务必恢复正常模式。
