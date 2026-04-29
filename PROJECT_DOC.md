# LCD-1.69 项目文档

## 项目概述

基于 ESP-IDF 的嵌入式 GUI 项目，使用 ST7789 驱动 1.69 英寸 LCD 屏幕（240×280），通过 LVGL v9 显示手部图像，UART 接收外部传感器数据，并以热力图阵点实时可视化压力分布。

## 项目结构

```
LCD-1.69/
├── CMakeLists.txt                    # 根构建配置
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml             # 依赖声明
│   └── main.c                        # 应用入口
├── components/
│   └── BSP/
│       ├── CMakeLists.txt            # BSP 组件构建配置
│       ├── LCD/
│       │   ├── lcd.h                 # LCD 引脚定义与接口声明
│       │   └── lcd.c                 # ST7789 SPI 初始化与背光控制
│       ├── LVGL/
│       │   ├── lvgl_ui.h             # LVGL UI 配置宏与接口
│       │   ├── lvgl_ui.c             # LVGL 初始化 + 触摸 + 手部图像显示
│       │   ├── ui_matrix.h           # 阵点可视化接口
│       │   ├── ui_matrix.c           # 手掌形状阵点热力图渲染
│       │   ├── hand_map.c            # 左手位图数据（约 622KB）
│       │   └── hand_map_right.c      # 右手位图数据（约 622KB）
│       └── UART/
│           ├── uart.h                # UART 引脚定义与接口声明
│           ├── uart.c                # UART 初始化与读写封装
│           ├── uart_receive.h        # 接收任务接口
│           └── uart_receive.c        # 传感器数据帧解析
├── managed_components/               # ESP-IDF 组件管理器依赖（未纳入版本控制）
└── sdkconfig                         # ESP-IDF 项目配置
```

## 硬件引脚定义

### SPI LCD 接口

| 引脚 | GPIO | 说明 |
|------|------|------|
| CLK  | 13   | SPI 时钟 |
| MOSI | 14   | SPI 数据输出 |
| MISO | -    | 未使用 |
| DC   | 11   | 数据/命令选择 |
| CS   | 12   | 片选 |
| RST  | 15   | 复位 |
| BLK  | 10   | 背光控制（高电平点亮） |

### UART 接口

| 引脚 | GPIO | 说明 |
|------|------|------|
| TX   | 34   | UART2 发送 |
| RX   | 35   | UART2 接收 |

### I2C 触摸接口 (CST816T)

| 引脚 | GPIO | 说明 |
|------|------|------|
| SDA  | 17   | I2C 数据 |
| SCL  | 18   | I2C 时钟 |
| RST  | NC   | 与 LCD 共用 GPIO15，不复位 |
| INT  | 16   | 触摸中断 |

## 软件架构

### 启动流程

```
app_main()
  ├── app_lvgl_ui_init()          # 初始化 LCD + LVGL + 触摸 + 显示 UI
  │     ├── init_lcd_spi()        #   初始化 SPI 总线
  │     ├── init_display()        #   初始化 ST7789 面板 + 点亮背光
  │     └── app_lvgl_init()       #   初始化 LVGL 库 + 注册显示设备
  │           ├── app_touch_init()     #   初始化 I2C + CST816T 触摸
  │           └── lvgl_port_add_touch() #  注册触摸输入设备
  │           ├── 创建图像对象，显示 hand_map 位图（左右手宏切换）
  │           ├── ui_matrix_create()  #   在手图上叠加手掌阵点层
  │           └── 创建 ZERO 调零按钮
  ├── usart_init(2000000)         # 初始化 UART2（2M 波特率）
  ├── xTaskCreate(uart_receive_task)  # 先创建接收任务（确保接收端就绪再发指令）
  ├── vTaskDelay(100ms)           # 等接收任务就绪
  └── send_start_cmd()            # 发送启动指令给外部设备
```

### LVGL 显示配置

| 参数 | 值 |
|------|-----|
| 水平分辨率 | 240 px |
| 垂直分辨率 | 280 px |
| 色彩格式 | RGB565 (16-bit) |
| 绘制缓冲区高度 | 70 行 |
| 双缓冲 | 开启 |
| DMA 传输 | 开启 |
| 字节交换 | 开启 |
| LVGL 任务优先级 | 4 |
| LVGL 任务栈 | 8192 字节 |
| LVGL 任务 tick | 5 ms |
| 触摸设备 | CST816T (I2C addr 0x15) |

### UART 数据协议

- **波特率**: 2,000,000 bps
- **数据位**: 8
- **停止位**: 1
- **校验**: 无
- **流控**: 无

#### 帧格式

| 字段 | 偏移 | 长度 | 说明 |
|------|------|------|------|
| 帧头 | 0 | 2 字节 | 固定 `0xFF 0xFF` |
| ... | 2 | 14 字节 | 未使用 |
| 协议标识 | 16 | 2 字节 | UDP_11 = `0x00 0x12` |
| ... | 18 | 2 字节 | 未使用 |
| 传感器数据 | 20 | 192 字节 | 32 个传感器值，每值 6 字节 |
| ... | 212 | ... | 帧尾 |

帧总长: **212 字节**

#### 传感器数据编码

每个传感器值占 6 字节：
- 前 3 字节：整数部分（大端序）
- 后 3 字节：小数部分（大端序，除以 1,000,000）

```c
float raw = int_part + dec_part / 1000000.0f;
sensor_value[i] = raw - zero_offset[i];
```

**调零校准**: 按下屏幕 ZERO 按钮后，当前各通道 raw_value 被记录为 `zero_offset`，后续显示增量值（raw - offset）。

#### 启动指令

18 字节固定指令：
- `[0..3]`: `0xFF 0xFF 0x06 0x09`
- `[4..16]`: `0x00`（保留）
- `[17]`: `0x11`

#### 帧同步策略

1. 循环缓冲区（1024 字节）累积数据
2. 搜索帧头 `0xFF 0xFF`
3. 不足 212 字节时等待更多数据
4. 验证协议标识（偏移 16-17）
5. 匹配成功则解析 32 个传感器 float 值
6. 调用 `ui_matrix_update()` 刷新阵点热力图
7. 匹配失败则丢弃 1 字节，重新搜索帧头
8. 缓冲区溢出时保留最近数据并尝试对齐帧头，而非清空全部
9. 启动时清空 UART 驱动层缓冲区，排除上电噪声
10. 超过 3 秒未收到有效帧则自动重发启动指令

### 阵点可视化（ui_matrix）

在 hand_map 手图之上叠加 29 个彩色圆点（通道 20-22 未使用），通过热力图色阶实时反映传感器压力值。

**坐标系统**: `gx`/`gy` 为 `float` 类型（支持小数精细调节），cell = 20px（16px 圆点 + 4px 间距），实际画布坐标 = `origin + (gx/gx - min) * cell`。

**阵点物理布局**（手掌形状，已按实际位置微调）：

```
        手掌 (4×4 方形，整数坐标):
        ch2(0,0)   ch15(1,0)   ch19(2,0)   ch28(3,0)
        ch3(0,1)   ch8 (1,1)   ch18(2,1)   ch27(3,1)
        ch4(0,2)   ch9 (1,2)   ch17(2,2)   ch26(3,2)
        ch5(0,3)   ch10(1,3)   ch16(2,3)   ch25(3,3)

        五指 (小数坐标，已微调):
        拇指:       ch7(-3.30, 0)   ch6(-2.20, 1)
        食指:       ch1(-1.00, -4.70)  ch0(-0.80, -3.15)  ch11(-0.60, -1.90)
        中指:       ch14(1.35, -5.55)   ch13(1.15, -3.85)   ch12(1.10, -2.45)
        无名指:     ch31(2.95, -4.70)   ch30(2.65, -3.20)   ch29(2.50, -2.00)
        小指侧:     ch23(4.80, -3.00)   ch24(4.40, -1.50)
```

| 参数 | 值 |
|------|-----|
| 通道数 | 29（原 32 通道，去除 20,21,22） |
| 坐标类型 | `float`（支持 0.5 格精细偏移） |
| 圆点直径 | 16 px |
| 圆点间距 | 4 px |
| 网格计算范围 | gx: -1~4（6列）, gy: -3~3（7行）* |
| 手掌区域 | 4×4 方形均匀网格，整数坐标不动 |
| 五指区域 | 小数坐标独立调节，代码按区域分段 |
| 色阶 | `#F5E5E5` (浅粉) → `#FF0000` (正红) |
| 值域映射 | 固定阈值 [500, 800] → [0, 255]（校准后可降低） |
| 刷新策略 | `lv_obj_set_style` 标记脏区，LVGL 定时任务（5ms 周期）自动刷新 |
| 滚动条 | 已关闭 `LV_SCROLLBAR_MODE_OFF`（消除圆点边缘横杠） |

> *实际五指坐标超出此范围（gx 可达 -3.3~4.8，gy 可达 -5.55），超出部分会被裁剪到屏幕外。

### 左右手模式切换

在 [lvgl_ui.h](components/BSP/LVGL/lvgl_ui.h) 中通过宏切换：

```c
// #define HAND_RIGHT  // 注释 = 左手，取消注释 = 右手
```

| 模式 | 位图源 | 阵点 gx 坐标 |
|------|--------|-------------|
| 左手 (默认) | `hand_map` | 原始坐标（已调试） |
| 右手 | `hand_map_right` | 镜像 `gx = 3.0f - gx`（绕手掌中心水平翻转） |

右手模式切换内容：
- `lvgl_ui.c`: 条件使用 `hand_map_right` 位图
- `ui_matrix.c`: 条件使用右手 `points[]` 数组（gx 已镜像，通道号待手动调整）
- `origin_x` 反向偏移（左手 +20，右手 -20）适应拇指位置

## 依赖项

| 依赖 | 版本 | 说明 |
|------|------|------|
| ESP-IDF | >= 4.1.0 | 框架 |
| lvgl/lvgl | ^9.5.0 | LVGL 图形库 |
| espressif/esp_lvgl_port | ^2.7.2 | ESP LVGL 端口层 |
| jbrilha/esp_lcd_st7789 | ^1.0.2 | ST7789 驱动 |
| espressif/esp_lcd_touch_cst816s | ^1.1.1~1 | CST816S 触摸驱动 |

## 构建优化

BSP 组件编译选项：
- `-ffast-math` — 浮点运算优化
- `-O3` — 最高等级优化
- `-Wno-error=format` / `-Wno-format` — 忽略格式警告

## 关键行为说明

1. **背光控制**: LCD 初始化时自动点亮背光，提供 `lcd_backlight_on/off/set` 接口手动控制
2. **镜像显示**: 面板配置了水平+垂直镜像翻转（`mirror(true, true)`）
3. **色序反转**: ST7789 需要反转颜色输出
4. **垂直间隙**: 面板底部留 20 行间隙（`set_gap(0, 20)`），适配 240×280 分辨率
5. **LVGL 线程安全**: UI 操作需通过 `lvgl_port_lock(pdMS_TO_TICKS(20))` 加锁，锁超时 20ms，拿不到锁则跳过本次更新等待下一帧；旧代码使用 `timeout=0` 存在竞态
6. **UART 任务栈**: 分配 8192 字节，避免 LVGL 操作导致栈溢出；不应在非 LVGL 任务中调用 `lv_refr_now()`
7. **启动时序**: 接收任务先于启动指令创建，确保指令回复不丢失；任务启动时清空 UART 驱动层缓冲区
8. **超时重发**: 若连续 3 秒未收到有效数据帧，自动重发启动指令唤醒外部设备
9. **触摸输入**: CST816T 通过 I2C (GPIO17/18) 连接到 LVGL，RST 与 LCD 共用 GPIO15
10. **调零校准**: 屏幕右下角 ZERO 按钮（70×40），按下后将当前各通道 raw_value 记录为零点偏移，后续显示 `sensor = raw - offset`
11. **数据校准流程**: `parse_udp11()` 中先计算原始值保存到 `raw_value[]`，再减去 `zero_offset[]` 得到 `sensor_value[]`，最后传入 `ui_matrix_update()`
12. **LVGL 任务栈**: 8192 字节（自 4096 扩大），承载触摸事件处理、按钮回调、printf 等调用链

## 已知问题与修复记录

### 修复 1：缓冲区溢出清空导致帧错位（2026-04-29）

**现象**: 传感器数据偶尔出现异常大值（百万级），大部分帧正常但个别帧后半段数据异常

**根因**: UART 消费速度跟不上接收速度时，`uart_len + len > 1024` 触发 `uart_len = 0` 直接丢弃全部缓冲数据。下一批数据进来后可能从非帧头位置开始解析，导致读取到错误偏移的字节

**修复**: [uart_receive.c](components/BSP/UART/uart_receive.c#L164-L176) — 溢出时仅丢弃最老的数据腾出空间，并尝试在溢出位置附近搜索帧头重新对齐，保留最近的有效数据

### 修复 2：LVGL 锁竞态（2026-04-29）

**现象**: LVGL 渲染期间，UART 任务可能在未持有锁的情况下操作 UI 对象，导致显示异常或崩溃

**修复**: [uart_receive.c](components/BSP/UART/uart_receive.c#L66) — `lvgl_port_lock(0)` 改为 `lvgl_port_lock(pdMS_TO_TICKS(20))`，增加返回值检查，拿不到锁则安全跳过本次更新

### 修复 3：显示屏偶尔无法接收数据（2026-04-29）

**现象**: 上电后偶尔不显示数据，需重启几次才正常

**根因**:
- UART 比接收任务早初始化，RX 脚浮空产生的噪声字节填充驱动缓冲区
- 启动指令在接收任务创建前发送，外部设备响应数据到达时无人读取
- 无超时重试，若启动指令丢失则永远收不到数据

**修复**:
- [main.c](main/main.c#L29-L53) — 先创建接收任务再发送启动指令，确保接收端就绪
- [uart_receive.c](components/BSP/UART/uart_receive.c#L154) — 任务启动时 `uart_flush_input()` 清空上电噪声
- [uart_receive.c](components/BSP/UART/uart_receive.c#L186-L189) — 3 秒无数据自动重发启动指令

### 修复 4：阵点布局重构为手掌形状（2026-04-29）

**背景**: 原 8×4 矩形网格与实际传感器物理布局（手掌形状）不匹配

**改动**:
- 通道数从 32 减至 29（去除 ch20-22）
- `gx`/`gy` 从 `int8_t` 改为 `float`，支持半格精细偏移
- 手掌 4×4 方形区域保持整数坐标不动
- 五指区域独立分组，各使用小数坐标微调至对应物理位置
- 代码按 `手掌/拇指/食指/中指/无名指/小指侧` 分段，修改时互不影响

### 修复 5：左右手模式宏切换（2026-04-29）

**背景**: 左手布局调试完毕，需支持右手贴片布局

**改动**:
- [lvgl_ui.h](components/BSP/LVGL/lvgl_ui.h#L11) — 新增 `HAND_RIGHT` 宏（默认注释，左手模式）
- [hand_map_right.c](components/BSP/LVGL/hand_map_right.c) — 符号重命名为 `hand_map_right`
- [lvgl_ui.c](components/BSP/LVGL/lvgl_ui.c) — `#ifdef` 条件选择位图源
- [ui_matrix.c](components/BSP/LVGL/ui_matrix.c#L19-L119) — 双 `points[]` 数组，右手 gx 坐标镜像 `3.0f - gx`，`origin_x` 反向偏移
- [ui_matrix.h](components/BSP/LVGL/ui_matrix.h#L4) — 添加 `#include "lvgl_ui.h"` 使宏可见

### 新增 1：CST816T 触摸 + 调零按钮（2026-04-29）

**内容**:
- 添加 `espressif/esp_lcd_touch_cst816s` 依赖
- [lvgl_ui.c](components/BSP/LVGL/lvgl_ui.c) — I2C 初始化 + CST816S 触摸注册 + 右下角 ZERO 调零按钮
- [uart_receive.c](components/BSP/UART/uart_receive.c) — 新增 `raw_value[32]` / `zero_offset[32]`，`uart_zero_calibrate()` 校准函数
- [uart_receive.h](components/BSP/UART/uart_receive.h) — 导出 `uart_zero_calibrate()`

### 修复 6：ZERO 按钮间歇性重启（2026-04-29）

**现象**: 点击 ZERO 按钮有时正常有时系统重启

**根因**: LVGL 任务栈 4096 字节不足，触摸事件 + 按钮回调 + `printf()` 峰值栈超限

**修复**: [lvgl_ui.c](components/BSP/LVGL/lvgl_ui.c#L67) — 任务栈 4096 → 8192

### 优化 1：热力图色阶（2026-04-29）

- 色阶从蓝→青→绿→黄→红改为 `#F5E5E5` (浅粉) → `#FF0000` (正红) 单色渐变
- 圆点停用滚动条 `LV_SCROLLBAR_MODE_OFF` 消除 9/12 点钟方向横杠
