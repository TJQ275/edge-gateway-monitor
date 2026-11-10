# 预编译固件

这两个 hex 由仓库源码编译生成，方便没有 Keil 的人先烧录验证硬件。

| 文件 | 烧录地址 | 大小 |
|---|---|---|
| `Bootloader.hex` | `0x08000000` | 约 6 KB |
| `F4project.hex` | `0x0800C000` | 约 203 KB |

## 烧录顺序

1. 先烧 **Bootloader.hex**
2. 再烧 **F4project.hex**

## 用 STM32CubeProgrammer

选中 hex 文件直接下载即可——hex 自带地址信息，工具会自动写到正确位置。

## 用 ST-Link Utility

菜单 Target → Program & Verify，选择 hex，地址会自动识别。

## 注意事项

- **不要用"全片擦除"**：擦掉 Bootloader 后应用无法启动
- 烧完 App 后第一次上电：元数据为空但应用有效，Bootloader 会直接跳转应用
- 这两个文件是构建产物，不保证与最新提交同步；需要最新版本请自行编译
