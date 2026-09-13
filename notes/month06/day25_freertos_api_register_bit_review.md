# 第六阶段 Day25：环境监测项目、FreeRTOS API、寄存器与位运算复习

## 一、当天完成内容

- 根据当前源码统一环境监测项目的真实任务数量、优先级和职责。
- 复习Queue、Mutex、Event Group、临界区和任务调度。
- 复习IWDG寄存器、超时时间及任务健康监控。
- 复习DHT11微秒级时序、超时退出、校验和及字节组装。
- 复习STM32F103 GPIO寄存器和DWT周期计数器。
- 练习MHz、Hz、ms、μs、Tick和字节等单位换算。
- 完成任务Deadline健康掩码和Tick回绕测试。
- 完成异或查找单独数字和二进制位计数练习。

## 二、项目真实任务划分

当前源码实际创建7个应用任务。

| 任务 | 优先级 | 主要职责 |
|---|---|---|
| sensorTask | AboveNormal | 读取DHT11、更新共享数据、判断报警、覆盖写入队列、更新业务事件和心跳 |
| ledTask | Normal | 读取传感器快照并刷新OLED |
| keyTask | Normal | 10ms轮询按键、20ms非阻塞消抖、修改阈值并保存Flash |
| uartTask | Low | 阻塞等待队列数据，通过USART1输出日志 |
| monitorTask | Low | 监控任务超时和业务事件变化 |
| watchdogTask | Low | 每3秒收集任务健康状态，决定是否刷新IWDG |
| wifiTask | Low | 独占USART2，执行ESP8266 AT、Wi-Fi和TCP通信流程 |

当前需要准确说明：

- watchdogTask当前是Low优先级，不是最高优先级。
- 实际负责刷新IWDG的是watchdogTask，不是monitorTask。
- README中的displayTask对应当前ledTask的显示职责。
- 重复的espTask已经删除，只保留wifiTask。
- ESP8266源码已有AT、Wi-Fi、TCP连接和发送流程，但MQTT上传尚未完成。
- 部分板载故障注入和网络链路测试仍待完成。

## 三、FreeRTOS同步机制

### 1. Queue

```c
g_sensor_msg_queue =
    xQueueCreate(1U, sizeof(SensorData));