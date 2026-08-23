# Day 5：GPIO、按键轮询、消抖与EXTI复习

## 一、今日目标

1. 复习GPIO输入输出模式。
2. 理解上拉输入、推挽输出和开漏输出。
3. 审计环境监测项目的真实按键实现。
4. 对比任务轮询与EXTI外部中断。
5. 将阻塞式按键消抖重构为非阻塞状态机。
6. 复习ISR、volatile和FreeRTOS FromISR API。

---

## 二、真实项目GPIO审计

环境监测项目使用STM32F103C8T6，包含三个按键：

| 按键 | GPIO |
|---|---|
| KEY_MODE | PB0 |
| KEY_UP | PB1 |
| KEY_DOWN | PB10 |

`gpio.c`中的配置为：

```c
GPIO_InitStruct.Pin =
    KEY_MODE_Pin | KEY_UP_Pin | KEY_DOWN_Pin;

GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
GPIO_InitStruct.Pull = GPIO_PULLUP;

HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);