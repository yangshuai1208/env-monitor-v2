# Day08：FreeRTOS任务健康监控与IWDG联动

## 一、今日目标

结合环境监测项目，重构任务健康监控机制，使独立看门狗只有在关键任务全部健康时才被刷新。

## 二、原方案的问题

原方案每3秒比较一次任务计数器是否变化。

但传感器任务周期约为10秒，因此在第3秒、第6秒和第9秒检查时，计数器可能没有变化，容易把正常周期任务误判为卡死。

任务健康不能只判断“本轮计数器是否变化”，应记录任务最后一次正常运行的Tick，并为不同任务设置独立Deadline。

## 三、TaskHealth结构

```c
typedef struct
{
    uint32_t last_alive_tick;
    uint32_t deadline_ticks;
    bool seen;
} TaskHealth;