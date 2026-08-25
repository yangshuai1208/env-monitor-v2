# Day7：DHT11、I²C/OLED 与驱动可靠性复习

## 1. 今日复习目标

- 理解嵌入式驱动分层
- 掌握 DHT11 单总线通信流程
- 掌握驱动超时、校验和错误传播
- 区分 DHT11 单总线与 I²C
- 理解 OLED 共享总线的 Mutex 保护
- 掌握传感器平均值计算
- 掌握定长环形缓冲区
- 审查项目中的看门狗误复位风险

---

## 2. 项目驱动分层

项目可以划分为四层：

1. 业务层：FreeRTOS 任务、报警判断、数据显示、网络上传
2. 设备驱动层：DHT11、OLED、ESP8266 等设备驱动
3. HAL 层：GPIO、I²C、UART 等 STM32 HAL API
4. 硬件层：STM32F103、传感器、OLED、WiFi 模块

例如 DHT11 数据链路：

```text
sensorTask
    ↓
DHT11_ReadData()
    ↓
GPIO输入输出、微秒计时
    ↓
DHT11硬件