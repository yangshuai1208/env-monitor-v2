
---

### 3. docs/hardware.md 内容

复制到 `docs/hardware.md`：

```markdown
# 硬件说明

## 硬件清单

| 硬件模块 | 作用 |
|---|---|
| STM32F103C8T6 | 主控芯片 |
| DHT11 | 采集温湿度 |
| OLED | 显示环境数据 |
| EC11 编码器 | 页面切换和交互 |
| W25Q64 | 保存历史环境数据 |
| LED | 环境状态提示 |
| 蜂鸣器 | 异常报警 |
| DAPLink / ST-Link | 下载和调试 |
| CH340 | 串口调试 |

## 接口说明

| 模块 | 接口 |
|---|---|
| OLED | I2C |
| W25Q64 | SPI |
| DHT11 | GPIO 单总线 |
| EC11 | GPIO / 外部中断 |
| LED | GPIO |
| 蜂鸣器 | GPIO |
| 串口调试 | UART |

## 注意事项

- OLED 的 SDA / SCL 不要接反。
- W25Q64 使用 SPI，CS 引脚需要单独控制。
- DHT11 对时序要求较高，建议使用 DWT 延时。
- 蜂鸣器注意区分有源蜂鸣器和无源蜂鸣器。
- 下载程序前确认 BOOT 引脚配置正确。