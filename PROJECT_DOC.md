# LCD-1.69 项目文档

更新日期：2026-05-06

## 项目概述

本项目是基于 ESP-IDF 5.3.3 和 ESP32-S3 的嵌入式触觉显示与电刺激反馈工程。系统使用 ST7789 驱动 1.69 英寸 240x280 LCD，通过 LVGL v9 显示手部位图和实时压力热力点；通过 UART2 接收外部 32 路压力传感器数据；通过 HV2801、DAC80502 和电位器 ADC 实现对应点位的电刺激反馈。

当前项目以右手模式为有效基线，`HAND_RIGHT` 已开启。右手通道映射已确认正确；左手分支暂未维护，后续如需使用应重新校准。

## 项目结构

```text
LCD-1.69/
├── CMakeLists.txt
├── dependencies.lock
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── main.c
├── components/
│   └── BSP/
│       ├── CMakeLists.txt
│       ├── LCD/
│       │   ├── lcd.h
│       │   └── lcd.c
│       ├── LVGL/
│       │   ├── lvgl_ui.h
│       │   ├── lvgl_ui.c
│       │   ├── ui_matrix.h
│       │   ├── ui_matrix.c
│       │   ├── hand_map.c
│       │   └── hand_map_right.c
│       ├── UART/
│       │   ├── uart.h
│       │   ├── uart.c
│       │   ├── uart_receive.h
│       │   └── uart_receive.c
│       └── Stim/
│           ├── stim.h
│           ├── stim.c
│           ├── dac80502.h
│           ├── dac80502.c
│           ├── stim_adc.h
│           └── stim_adc.c
├── managed_components/
├── partitions.csv
└── sdkconfig
```

## 硬件连接

### LCD: ST7789

| 信号 | GPIO | 说明 |
|------|------|------|
| CLK | GPIO13 | SPI 时钟 |
| MOSI | GPIO14 | SPI 数据输出 |
| MISO | - | 未使用 |
| DC | GPIO11 | 数据/命令选择 |
| CS | GPIO12 | 片选 |
| RST | GPIO15 | 复位 |
| BLK | GPIO10 | 背光控制，高电平点亮 |

LCD 使用 `SPI2_HOST`，像素时钟当前设置为 60 MHz。

### 触摸: CST816S/CST816T

| 信号 | GPIO | 说明 |
|------|------|------|
| SDA | GPIO17 | I2C 数据 |
| SCL | GPIO18 | I2C 时钟 |
| INT | GPIO16 | 触摸中断 |
| RST | NC | 当前不单独复位，LCD RST 使用 GPIO15 |

触摸注册到 LVGL 输入设备，用于 ZERO 调零按钮和 STIM 开关。

### UART2 传感器数据

| 信号 | GPIO | 说明 |
|------|------|------|
| TX | GPIO34 | UART2 发送 |
| RX | GPIO35 | UART2 接收 |

UART 参数：

- 波特率：2,000,000 bps
- 数据位：8
- 停止位：1
- 校验：无
- 流控：无

### HV2801 电刺激通道选择

HV2801 引出 32 个输出通道，当前按完整 32 bit mask 移位输出。硬件通道号与传感器通道号一致：

```c
stim_ch = sensor_ch;
mask = 1UL << stim_ch;
```

| 信号 | GPIO | 说明 |
|------|------|------|
| HV_CLR | GPIO40 | MTDO |
| HV_CS | GPIO39 | MTCK |
| HV_CLK | GPIO38 | 移位时钟 |
| HV_DIN | GPIO37 | 数据输入 |
| HV_DOUT | GPIO36 | 数据输出/读回，当前仅配置为输入 |

### DAC80502 电流幅值控制

DAC80502 通过 GPIO 模拟 24 bit SPI，参考 STM32 项目 `H723_ALL_IN_ONE` 的实现方式。

| 信号 | 原理图网络 | GPIO | 说明 |
|------|------------|------|------|
| SCK | SPICLK_P | GPIO47 | DAC 时钟 |
| MOSI | SPICLK_N | GPIO48 | DAC 数据输入 |
| CS/SYNC | SPICS1 | GPIO26 | DAC 片选/同步 |

DAC80502 当前初始化行为：

- GAIN 寄存器 `0x04` 写入 `0x0103`
- DAC A 输出 `600 mV`，作为空闲电流镜控制电压
- DAC B 输出 `52 mV`，按参考项目保留

### 电位器 ADC

| 信号 | GPIO | ADC |
|------|------|-----|
| 电位器输出 | GPIO3 | ADC_UNIT_1 / ADC_CHANNEL_2 |

当前使用 ESP-IDF oneshot ADC，12 dB 衰减。ADC raw 值按 `0~4095` 映射为 `sensitive 0~10`，用于控制刺激脉宽。

## 软件架构

### 启动流程

```text
app_main()
  ├── stim_init()
  │   ├── 初始化 HV2801 GPIO
  │   ├── dac80502_init()
  │   └── stim_adc_init()
  ├── app_lvgl_ui_init()
  │   ├── init_lcd_spi()
  │   ├── init_display()
  │   ├── app_lvgl_init()
  │   ├── 创建手部位图
  │   ├── ui_matrix_create()
  │   ├── 创建 ZERO 按钮
  │   └── 创建 STIM 开关
  ├── usart_init(2000000)
  ├── xTaskCreate(stim_task)
  ├── xTaskCreate(uart_receive_task)
  ├── vTaskDelay(100ms)
  └── send_start_cmd()
```

### 任务分工

| 任务/模块 | 职责 |
|----------|------|
| LVGL port task | 屏幕刷新和触摸事件处理 |
| `uart_receive_task` | 接收 UART 数据帧、解析 32 路传感器数据、刷新 UI 数据源 |
| `stim_task` | 按 25 Hz 周期执行电刺激脉冲输出 |
| `ui_matrix` | 在手图上渲染 29 个压力热力点 |
| `Stim` | 保存最新刺激目标、控制 HV2801/DAC80502/ADC |

### 数据流

```text
外部传感器设备
  -> UART2 数据帧
  -> parse_udp11()
  -> raw_value[32]
  -> zero_offset[32]
  -> sensor_value[32]
       ├── ui_matrix_update(sensor_value)
       └── stim_update(sensor_value)
              -> stim_task()
                   ├── DAC80502 设置电流幅值
                   ├── HV2801 打开最大压力通道
                   └── 按 ADC 电位器脉宽关闭通道
```

## LVGL 显示

| 参数 | 值 |
|------|-----|
| 分辨率 | 240 x 280 |
| 色彩格式 | RGB565 |
| 绘制缓冲区高度 | 70 行 |
| 双缓冲 | 开启 |
| DMA buffer | 开启 |
| RGB565 字节交换 | 开启 |
| LVGL 任务优先级 | 4 |
| LVGL 任务栈 | 8192 字节 |
| LVGL tick | 5 ms |
| 触摸设备 | CST816S/CST816T |

当前 UI 元素：

- 居中手部位图：由 `HAND_RIGHT` 宏选择 `hand_map_right` 或 `hand_map`
- 手部热力点：由 `ui_matrix_create()` 创建
- ZERO 按钮：右下角，点击后记录当前 `raw_value[32]` 为零点偏移
- STIM 开关：左下角，上电默认关闭，打开后才允许电刺激输出

## UART 数据协议

### 帧格式

| 字段 | 偏移 | 长度 | 说明 |
|------|------|------|------|
| 帧头 | 0 | 2 字节 | 固定 `0xFF 0xFF` |
| 保留 | 2 | 14 字节 | 当前未使用 |
| 协议标识 | 16 | 2 字节 | UDP_11 = `0x00 0x12` |
| 保留 | 18 | 2 字节 | 当前未使用 |
| 传感器数据 | 20 | 192 字节 | 32 个传感器值，每值 6 字节 |

帧总长：212 字节。

### 传感器值编码

每个传感器值占 6 字节：

- 前 3 字节：整数部分，大端序
- 后 3 字节：小数部分，大端序，除以 1,000,000

```c
float raw = int_part + dec_part / 1000000.0f;
sensor_value[i] = raw - zero_offset[i];
```

### 启动指令

18 字节固定指令：

```text
[0..3]  = FF FF 06 09
[4..16] = 00
[17]    = 11
```

### 帧同步策略

1. 使用 1024 字节缓冲区累积 UART 数据。
2. 搜索帧头 `0xFF 0xFF`。
3. 不足 212 字节时等待更多数据。
4. 检查偏移 16/17 的协议标识。
5. 匹配成功则解析 32 个传感器值。
6. 匹配失败则丢弃 1 字节并重新搜索。
7. 缓冲区溢出时保留最近数据，并尝试重新对齐帧头。
8. 启动时清空 UART 驱动层缓冲区，排除上电噪声。
9. 超过 3 秒未收到有效帧时自动重发启动指令。

## 阵点可视化

`ui_matrix.c` 在手部位图上叠加 29 个圆点。当前有效模式是右手模式；右手通道映射已经确认正确。

### 左手布局说明

左手分支保留历史布局，当前暂未维护。文档中旧的“通道 20/21/22 未使用”主要描述左手/旧布局，不作为右手模式判断依据。

### 热力图参数

| 参数 | 值 |
|------|-----|
| 显示点数 | 29 |
| 点直径 | 16 px |
| 点间距 | 4 px |
| 坐标类型 | `float` |
| 色阶 | `#F5E5E5` 浅粉 -> `#FF0000` 正红 |
| 显示阈值 | `500~800` 映射到 `0~255` |
| 滚动条 | `LV_SCROLLBAR_MODE_OFF` |

## 电刺激实现

### 控制原则

当前电刺激遵循参考项目 `H723_ALL_IN_ONE` 的方式：

- HV2801 只负责通道选择
- DAC80502 控制电流镜输入电压，即电流幅值
- GPIO3 电位器 ADC 控制刺激脉宽
- 传感器最大压力点决定刺激通道和 DAC 幅值

### 压力到 DAC 幅值

当前参数位于 `components/BSP/Stim/stim.c`：

```c
#define STIM_PRESSURE_MIN 500.0f
#define STIM_PRESSURE_MAX 800.0f
#define STIM_DAC_IDLE_MV 600
#define STIM_DAC_MIN_MV 650
#define STIM_DAC_MAX_MV 1050
```

映射关系：

```text
sensor <= 500      -> 无效刺激，DAC A = 600 mV
500 < sensor < 800 -> DAC A = 650~1050 mV 线性映射
sensor >= 800      -> DAC A = 1050 mV
```

如实际体感过强，应优先降低 `STIM_DAC_MAX_MV`。

### 电位器到脉宽

当前 ADC 映射：

```text
GPIO3 ADC raw 0~4095 -> sensitive 0~10
pulse_width_us = sensitive * 100 us
```

因此脉宽范围约为 `0~1000 us`。如果旋钮方向与预期相反，可在 `stim_adc_get_sensitive()` 中改为：

```c
sensitive = 10 - sensitive;
```

### 刺激时序

当前刺激频率约为 25 Hz：

```c
#define STIM_PERIOD_MS 40
```

每个 40 ms 周期：

1. 读取电位器 ADC，得到 `pulse_width_us`。
2. 如果 STIM 开关关闭、压力无效或脉宽为 0，则 HV2801 全关，DAC A 保持 600 mV。
3. 如果有效，则设置 DAC A 电压。
4. 打开最大压力通道。
5. 延时 `pulse_width_us`。
6. 关闭所有 HV2801 通道。

`stim_update()` 不直接输出刺激，只保存最新目标通道和 DAC 电压；实际 GPIO 操作由 `stim_task()` 执行。

## 依赖项

`main/idf_component.yml` 直接依赖：

| 依赖 | 版本 |
|------|------|
| ESP-IDF | 5.3.3（lock 文件记录） |
| `lvgl/lvgl` | 9.5.0 |
| `espressif/esp_lvgl_port` | 2.7.2 |
| `jbrilha/esp_lcd_st7789` | 1.0.2 |
| `espressif/esp_lcd_touch_cst816s` | 1.1.1~1 |

BSP 组件 CMake 显式依赖：

```cmake
driver
esp_driver_gpio
esp_timer
esp_adc
esp_rom
log
esp_lcd_st7789
lvgl
esp_lvgl_port
esp_lcd_touch
esp_lcd_touch_cst816s
```

## 构建与运行

在 ESP-IDF 5.3.3 终端中执行：

```powershell
idf.py build
idf.py flash monitor
```

当前普通 PowerShell 环境中 `idf.py` 不在 PATH 内，因此在非 ESP-IDF 终端无法完成本地编译验证。

## 测试建议

### 编译检查

1. 使用 ESP-IDF 终端运行 `idf.py build`。
2. 若 CMake 报组件解析错误，优先检查 `components/BSP/CMakeLists.txt` 的 `REQUIRES`。
3. 若 GPIO47/GPIO48 报错，确认 `sdkconfig` 目标为 `esp32s3`。

### 无负载电气测试

不要先接人体。建议先用示波器或逻辑分析仪确认：

- HV2801 `CLR/CS/CLK/DIN` 有正确 32 bit 时序
- `stim_open_ch(0)` 对应 bit0，`stim_open_ch(31)` 对应 bit31
- DAC80502 SCK/MOSI/SYNC 有 24 bit 写寄存器时序
- DAC A 空闲为 600 mV
- 压力升高时 DAC A 在 650~1050 mV 之间变化
- GPIO3 电位器 raw 值随旋钮变化，方向符合预期

### 假负载测试

1. STIM 开关保持关闭，确认 HV2801 全关、DAC A 约 600 mV。
2. 打开 STIM，输入一个超过阈值的传感器通道。
3. 确认只打开最大压力对应通道。
4. 调节电位器，确认脉宽从 0 到约 1000 us 变化。
5. 降低压力到阈值以下，确认 HV2801 全关、DAC A 回到 600 mV。

### 人体测试注意

人体测试前必须先确认假负载结果。初次人体测试建议：

- 将电位器调到最小脉宽
- 必要时先降低 `STIM_DAC_MAX_MV`
- 单点、短时间测试
- 保持 STIM 开关可随时关闭

## 已知限制与待验证项

- 当前未在本会话中完成 `idf.py build`，原因是普通 PowerShell 找不到 `idf.py`。
- DAC80502 的 GPIO 模拟 SPI 时序来自参考项目，需在实物上确认。
- GPIO3 ADC 当前未做电压校准，仅用于 0~10 档位映射。
- `HV_DOUT` 当前未用于移位链读回诊断。
- 左手模式暂未维护。
- 当前 `README.md` 仍是 ESP-IDF 示例模板，建议后续用本文档内容更新。

## 历史修复记录

### 2026-04-29：UART 缓冲区溢出导致帧错位

现象：传感器数据偶尔出现异常大值，部分帧后半段数据异常。

处理：缓冲区溢出时不再直接清空全部数据，而是丢弃最老数据并尝试重新搜索帧头。

### 2026-04-29：LVGL 锁竞态

现象：UART 任务可能在 LVGL 渲染期间直接操作 UI 对象。

处理：UI 更新使用 `lvgl_port_lock(pdMS_TO_TICKS(20))`，拿不到锁则跳过本次 UI 刷新。

### 2026-04-29：上电后偶尔收不到数据

原因：

- UART 初始化早于接收任务，RX 浮空噪声可能填充缓冲区
- 启动指令可能在接收任务就绪前发出
- 无超时重发机制

处理：

- 接收任务先于启动指令创建
- 启动时 `uart_flush_input()`
- 3 秒无有效帧自动重发启动指令

### 2026-04-29：阵点布局重构

将原矩形网格改为手掌形状布局，`gx/gy` 改为 `float` 以支持精细调节。

### 2026-04-29：CST816S 触摸与 ZERO 调零

新增触摸注册、ZERO 按钮、`raw_value[32]`、`zero_offset[32]` 和 `uart_zero_calibrate()`。

### 2026-04-29：ZERO 按钮触发重启

原因：LVGL 任务栈 4096 字节不足。

处理：LVGL 任务栈扩大到 8192 字节。

### 2026-05-06：电刺激反馈集成

新增 `Stim` 模块，集成 HV2801、DAC80502 和 GPIO3 ADC。刺激方式从“压力控制粗占空比”优化为“压力控制 DAC 电流幅值，电位器控制脉宽，25 Hz 周期输出”。
