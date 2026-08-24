# Day 6：传感器数据模型模块化与 FreeRTOS 工程重构

## 一、今日目标

今天结合 STM32F103 环境监测系统，对传感器数据结构、报警判断和 FreeRTOS 任务间数据传递进行模块化重构。

主要目标：

1. 将传感器数据结构从 `freertos.c` 中抽离。
2. 将报警判断逻辑封装为独立模块。
3. 删除重复的 `SensorData` 和 `SensorMessage`。
4. 统一队列、显示任务和串口任务的数据类型。
5. 修复配置快照失败时使用未初始化数据的问题。
6. 优化传感器数据快照和按键扫描周期。

---

## 二、重构前的问题

原工程把以下内容全部放在 `freertos.c` 中：

- `AlarmState` 报警枚举。
- `SensorData` 传感器状态结构体。
- `SensorMessage` 队列消息结构体。
- `CheckAlarmState()` 报警判断函数。
- `AlarmStateToString()` 报警字符串转换函数。

这会产生以下问题：

1. `freertos.c` 同时负责任务调度、数据模型和业务逻辑，职责过多。
2. `SensorData` 与 `SensorMessage` 字段高度重复。
3. 增加字段时需要同时修改多个结构体和大量复制代码。
4. 报警逻辑无法脱离 STM32 工程进行单元测试。
5. 后续扩展其他传感器时维护成本较高。

---

## 三、模块划分

重构后分为三个层次。

### 1. 数据模型层：sensor_data.h

负责定义：

- `AlarmState` 报警状态。
- `SensorData` 传感器完整数据结构。
- 模块对外函数声明。

```c
typedef enum
{
    ALARM_NONE = 0,
    ALARM_TEMP_HIGH,
    ALARM_HUMI_LOW,
    ALARM_BOTH
} AlarmState;