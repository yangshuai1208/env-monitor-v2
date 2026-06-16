# FreeRTOS 任务划分说明

## 一、任务划分总览

| 任务名称 | 功能 | 建议周期 | 优先级 |
|---|---|---:|---:|
| sensor_task | 读取 DHT11 温湿度 | 1000ms | 中 |
| oled_task | 刷新 OLED 页面 | 300ms | 中 |
| uart_task | 串口输出调试信息 | 1000ms | 低 |
| alarm_task | 判断环境状态并控制 LED / 蜂鸣器 | 500ms | 中 |
| storage_task | 保存历史数据到 W25Q64 | 5000ms | 低 |

## 二、sensor_task

`sensor_task` 是数据源任务，负责周期性读取 DHT11 温湿度数据。

主要职责：

- 初始化 DHT11
- 周期性读取温度和湿度
- 判断读取是否成功
- 将温湿度数据封装成结构体
- 通过 FreeRTOS 队列发送给其他任务

伪代码：

```c
typedef struct
{
    float temperature;
    float humidity;
    uint8_t valid;
} EnvData_t;

void sensor_task(void *argument)
{
    EnvData_t env_data;

    while (1)
    {
        if (DHT11_Read(&env_data.temperature, &env_data.humidity) == 0)
        {
            env_data.valid = 1;
            xQueueSend(envDataQueueHandle, &env_data, 0);
        }
        else
        {
            env_data.valid = 0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
```

## 三、oled_task

`oled_task` 负责 OLED 页面刷新。

主要职责：

- 接收温湿度数据
- 显示温度、湿度
- 显示当前页面
- 显示环境状态
- 根据 EC11 / 按键切换页面

## 四、uart_task

`uart_task` 负责串口调试输出。

主要职责：

- 输出温湿度数据
- 输出任务运行状态
- 输出异常信息
- 辅助调试传感器和系统状态

示例输出：

```text
[ENV] temp=25.3C humi=48.0% status=OK
```

## 五、alarm_task

`alarm_task` 负责环境状态判断。

主要职责：

- 判断温度是否过高
- 判断湿度是否异常
- 控制 LED 显示舒适度状态
- 控制蜂鸣器报警

示例逻辑：

```text
温度 18~28℃ 且湿度 40~70%：舒适
温度过高：LED 警告 + 蜂鸣器提示
湿度异常：LED 提示
```

## 六、storage_task

`storage_task` 负责历史数据保存。

主要职责：

- 定时接收环境数据
- 将数据写入 W25Q64
- 保存历史温湿度记录
- 为后续历史曲线显示提供数据来源

## 七、为什么使用队列

使用队列的原因：

- 避免多个任务直接访问同一份全局变量
- 降低任务之间的耦合
- 保证数据传递有明确方向
- 方便后续扩展多个消费者任务

数据流：

```text
sensor_task
    ↓ xQueueSend
EnvData Queue
    ↓ xQueueReceive
oled_task / alarm_task / uart_task / storage_task
```

## 八、面试讲解重点

面试时可以这样讲：

本项目中，我将温湿度采集、OLED 显示、报警提示、串口输出和历史数据存储拆分为多个 FreeRTOS 任务。sensor_task 作为数据源任务，周期性读取 DHT11 数据，并通过队列将温湿度结构体传递给其他任务。这样可以避免所有功能堆在 while(1) 中，也降低了模块耦合度，使项目更容易维护和扩展。