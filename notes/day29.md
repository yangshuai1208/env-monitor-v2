# Day29 STM32 + FreeRTOS 项目复盘

## 1. 项目架构

STM32F103C8T6 + FreeRTOS

外设：
- DHT11
- OLED
- 按键
- UART
- 蜂鸣器/LED
- ESP8266 状态框架

## 2. 七个任务

sensorTask
displayTask
uartTask
keyTask
monitorTask
watchdogTask
espTask

## 3. FreeRTOS

Queue：
任务间传递数据

Mutex：
保护共享资源

Event Group：
表示多个系统状态

IWDG：
异常情况下恢复系统

## 4. UART

HAL_UART_Receive_IT
→ USART中断
→ HAL_UART_RxCpltCallback
→ 处理数据
→ 再次HAL_UART_Receive_IT

## 5. 调试问题

DHT11采样过快：
增加合理采样间隔。

系统卡顿：
查看日志
→ 查看任务状态
→ 检查阻塞时间
→ 检查优先级
→ 检查共享资源与死锁

## 6. 当前边界

ESP8266相关框架存在，
但完整MQTT云端上传没有完成。