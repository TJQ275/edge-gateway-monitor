# 工业边缘网关监控

基于 **STM32F411CEU6** 的边缘网关：现场总线采集、本地触摸界面、云端上报、断网缓存，以及 **Bootloader + XMODEM OTA**。

## 功能

- **采集**：Modbus RTU 主站轮询温湿度（0x01）与光照（0x02）
- **显示**：320×240 SPI 屏 + 触摸，LVGL 8.3.11 界面，空闲自动息屏
- **上云**：ESP8266 AT 透传 + 手写 MQTT 报文，上报巴法云
- **缓存**：断网时写入 W25Q64，联网后自动补传
- **对外**：第二路 RS485 作为 Modbus RTU 从站，供 PLC 读取
- **升级**：Bootloader 通过 XMODEM-CRC 接收固件，断电可恢复
- **可靠性**：独立看门狗任务、CRC 校验、升级标记

## 硬件

| 器件 | 型号 / 接口 |
|---|---|
| MCU | STM32F411CEU6，100 MHz，512 KB Flash / 128 KB RAM |
| 显示 | ILI9341 2.8"，SPI1 |
| 触摸 | XPT2046，与屏共用 SPI1 |
| 存储 | W25Q64 8 MB，SPI2 |
| 网络 | ESP8266（AT 固件），USART1 |
| 现场总线 | MAX3485 ×2，USART2 / USART6 |

### 引脚

| 功能 | 引脚 |
|---|---|
| USART1（ESP8266） | PA9 TX / PA10 RX |
| USART2（RS485-1，Modbus 主站） | PA2 TX / PA3 RX，DE = PB0 |
| USART6（RS485-2，Modbus 从站 / OTA） | PA11 TX / PA12 RX，DE = PB8 |
| SPI1（屏 + 触摸） | PA5 SCK / PA6 MISO / PA7 MOSI |
| 屏控制 | CS = PB6，DC = PB7，RST = PB5，背光 = PA8 |
| 触摸片选 | PB10 |
| SPI2（W25Q64） | PB13 SCK / PB14 MISO / PB15 MOSI，CS = PB12 |
| 状态灯 | PB3 / PB4 / PA15 |
| 调试 | SWD：PA13 / PA14 |

> MAX3485 必须使用 **3.3 V** 版本，5 V 版本会损坏 PA12。

## 软件栈

| 组件 | 版本 |
|---|---|
| HAL | STM32Cube FW_F4 V1.28.3（HAL 1.8.5） |
| RTOS | FreeRTOS V10.3.1 + CMSIS-RTOS v2，heap_4，20 KB |
| GUI | LVGL 8.3.11，16 bpp |
| 工具链 | Keil MDK 5，ARMCC V5.06 |

## 目录

```
Core/           CubeMX 生成 + 启动、时钟、外设
code/           LCD、触摸、任务体、OTA 触发
model/          Modbus 主站 / 从站协议
ESP8266/        AT 指令、环形缓冲、MQTT 报文
SPI_Flash/      W25Q64 驱动、日志与缓存分区
Lvgl/           LVGL 源码与页面（LVGL_myGUI）
bootloader/     OTA Bootloader 独立工程
MDK-ARM/        主工程
```

## 内存布局

| 地址 | 大小 | 用途 |
|---|---|---|
| 0x08000000 | 32 KB | Bootloader |
| 0x08008000 | 16 KB | OTA 元数据 |
| 0x0800C000 | 464 KB | 应用程序 |

## 编译

1. 准备凭据（**不会提交到仓库**）：

   ```
   copy ESP8266\secrets.example.h ESP8266\secrets.h
   ```

   然后填入自己的 WiFi 名称、密码和巴法云私钥（32 位）。

2. 打开 `bootloader/Bootloader.uvprojx` 编译 Bootloader
3. 打开 `MDK-ARM/F4project.uvprojx` 编译应用程序

## 烧录顺序

1. 先烧 **Bootloader**（0x08000000 ~ 0x08007FFF）
2. 再烧 **应用程序**（0x0800C000 起）
3. Keil → Options → Debug → Settings → Flash Download，选择 **Erase Sectors**（不要用 Erase Full Chip，否则会擦掉 Bootloader）

## OTA 升级

1. 串口助手连接 **RS485-2**，9600 8N1，发送触发帧：

   ```
   0A 06 00 FF 5A 5A 02 1A
   ```

2. 板子复位进入 Bootloader，把串口改为 **115200**，会持续收到字符 `C`
3. Keil 生成 bin：`fromelf --bin --output "$L@L.bin" "#L"`
4. Tera Term → File → Transfer → XMODEM → Send，选择 bin 文件

升级失败或断电后重新上电仍会停在下载模式，重新发送即可。

## Modbus 从站

站号 `0x0A`，功能码 03 / 04：

| 寄存器 | 内容 | 缩放 |
|---|---|---|
| 0 | 温度 | ×10 |
| 1 | 湿度 | ×10 |
| 2 | 光照（高 16 位） | — |
| 3 | 光照（低 16 位） | — |
| 4 | 网络状态 | 0/1 |

写寄存器 `0x00FF` = `0x5A5A` 触发升级。

## 已知限制

- 单 App 分区，**不支持回滚**，升级失败需重新发送
- Bootloader 不能自升级，只能通过 SWD 烧录
- OTA 目前走 RS485，尚未接入 WiFi 远程下载

## 许可

见仓库中的 LICENSE 文件。
