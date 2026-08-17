# 第六阶段 Day1：FreeRTOS 项目源码审计与复习


## 一、审计结论摘要

当前工程能够体现 FreeRTOS 任务拆分、Queue、Mutex、Event Group、静态/动态任务创建、任务心跳与 IWDG 等关键能力，但仓库说明和源码存在明显偏差。

最重要的结论：

1. **源码实际创建 8 个应用任务，不是 README 中的 7 个。**
2. `watchdogTask` 的真实优先级是 `osPriorityLow`，不是“最高优先级”。
3. `espTask` 与 `wifiTask` 都会初始化 ESP8266、连接 WiFi，并并发访问 USART2 和同一个接收缓冲区，存在职责重复和通信竞争。
4. `espTask`、`wifiTask` 的静态栈数组只有 `256` 个 `StackType_t`，线程定义却配置为 `384`；当前 CMSIS 包装层把该值直接传给 `xTaskCreateStatic()`，存在越界使用静态栈缓冲区的风险。
5. README 声称使用 `HAL_UART_Receive_IT()` 和 `HAL_UART_RxCpltCallback()`，但当前应用源码未找到这两个调用；ESP8266 实际使用阻塞式 `HAL_UART_Receive()` 轮询接收。
6. `monitorTask` 等待关键任务事件的超时是 6 秒，但 `sensorTask` 允许采样周期达到 10 秒，可能产生误报 `SYS TASK TIMEOUT`。
7. 已定义栈溢出和内存申请失败 Hook，但配置文件未启用对应宏，因此当前 Hook 默认不会生效。

## 二、FreeRTOS 实际配置

来源：`Core/Inc/FreeRTOSConfig.h`。

| 配置项 | 当前值 | 含义与审计结论 |
|---|---:|---|
| `configUSE_PREEMPTION` | 1 | 开启抢占式调度 |
| `configSUPPORT_STATIC_ALLOCATION` | 1 | 支持静态创建任务 |
| `configSUPPORT_DYNAMIC_ALLOCATION` | 1 | 支持动态创建任务 |
| `configTICK_RATE_HZ` | 1000 Hz | 1 Tick 约为 1 ms |
| `configMAX_PRIORITIES` | 7 | 内核支持 7 个优先级等级 |
| `configMINIMAL_STACK_SIZE` | 128 | 空闲任务最小栈深度配置 |
| `configTOTAL_HEAP_SIZE` | 3072 B | 动态任务与 RTOS 对象共享，余量需要实测 |
| `configUSE_MUTEXES` | 1 | 已启用互斥量 |
| `INCLUDE_vTaskDelay` | 1 | 可调用相对延时 |
| `INCLUDE_vTaskDelayUntil` | 0 | 当前不能直接使用 `vTaskDelayUntil()` |
| `configUSE_TIME_SLICING` | 未显式配置 | 当前中间件默认值为 1 |
| `configCHECK_FOR_STACK_OVERFLOW` | 未配置 | 中间件默认值为 0，栈溢出 Hook 未启用 |
| `configUSE_MALLOC_FAILED_HOOK` | 未配置 | 中间件默认值为 0，Malloc Hook 未启用 |

工程同时使用静态任务和动态任务，但 README 没有说明混合分配的设计理由。后续应记录各任务的栈高水位和剩余堆空间，再决定栈大小和分配方式。

## 三、源码实际创建的 8 个任务

来源：`Core/Src/freertos.c` 的 `MX_FREERTOS_Init()`。

| 源码任务名 | 真实职责 | 优先级 | 创建方式 | 栈配置值 | 周期/阻塞点 | 通信与共享资源 |
|---|---|---|---|---:|---|---|
| `ledTask` | 实际承担 OLED 显示任务职责 | Normal | 动态 | 256 | 每 1000 ms 刷新 | 读取传感器快照；OLED Mutex；设置 `EVT_DISPLAY_RUN` |
| `uartTask` | 接收传感器消息并输出 USART1 日志 | Low | 动态 | 128 | `xQueueReceive(..., portMAX_DELAY)` | Queue；配置 Mutex；USART1 Mutex；设置 `EVT_UART_RUN` |
| `sensorTask` | DHT11 采样、报警判断、发布最新数据 | AboveNormal | 动态 | 128 | 初始等待 1000 ms；随后按 1000～10000 ms 采样 | 配置 Mutex；临界区；Event Group；长度 1 Queue 覆盖写 |
| `watchdogTask` | 检查传感器/显示/UART 心跳并决定是否喂狗 | Low | 静态 | 256 | 初始等待 3000 ms；每 3000 ms 检查 | 临界区读取心跳；`HAL_IWDG_Refresh()` |
| `keyTask` | 按键扫描、阈值调整、Flash 保存 | Normal | 静态 | 256 | 每 50 ms 扫描；消抖中会延时 | 配置 Mutex；USART1 Mutex；Flash；设置 `EVT_CONFIG_SAVED` |
| `monitorTask` | 等待关键任务事件并输出系统状态 | Low | 静态 | 256 | Event Group 最多等待 6000 ms，之后再延时 1000 ms | Event Group；USART1 Mutex |
| `espTask` | ESP8266 AT 检测和 WiFi 连接 | Low | 静态 | **384** | 启动等待 5 秒，完成后长期 10 秒延时 | USART2；全局 ESP 接收缓冲；WiFi 事件；USART1 日志 |
| `wifiTask` | 重复 AT 检测/WiFi 连接，并增加 TCP 连接与数据发送 | Low | 静态 | **384** | 启动等待 5 秒；正常时每 5 秒发送 | USART2；全局 ESP 接收缓冲；传感器快照；USART1 日志 |

### 命名问题

README 使用 `displayTask`，源码实际名称是 `ledTask` / `StartLEDTask()`。功能上它确实是显示任务，但面试时必须说明源码真实名称，或者后续统一重命名。

### 优先级顺序

当前相对优先级为：

```text
sensorTask（AboveNormal）
        ↓
ledTask、keyTask（Normal）
        ↓
uartTask、watchdogTask、monitorTask、espTask、wifiTask（Low）
```

因此不能再说“看门狗任务优先级最高”。

`watchdogTask` 使用低优先级不一定必然错误：如果高优先级任务死循环导致它无法运行，停止喂狗反而能够触发复位。关键是 README 和面试表述必须与源码一致，并验证其调度时限。

## 四、任务状态在本项目中的实际体现

### 1. 运行态（Running）

单核 STM32F103 同一时刻只有一个任务真正执行。调度器从就绪任务中选择最高优先级任务运行。

### 2. 就绪态（Ready）

任务具备运行条件，但正在等待 CPU。例如多个 Low 优先级任务同时就绪时，由调度器轮转执行。

### 3. 阻塞态（Blocked）

项目中的典型阻塞点：

- `osDelay(...)`：等待时间到达。
- `xQueueReceive(..., portMAX_DELAY)`：`uartTask` 等待传感器消息。
- `xSemaphoreTake(..., timeout)`：等待串口、OLED 或配置互斥量。
- `xEventGroupWaitBits(..., 6000 ms)`：`monitorTask` 等待三项任务运行事件。

阻塞期间任务不占用 CPU，这比 `while (flag == 0)` 忙等更合理。

### 4. 挂起态（Suspended）

当前应用代码未使用 `vTaskSuspend()` / `vTaskResume()`。

不要把延时或等待队列误称为“挂起”，它们属于阻塞态。

## 五、任务间通信与资源保护

### 1. 消息队列

```c
g_sensor_msg_queue = xQueueCreate(1, sizeof(SensorMessage));
```

数据流：

```text
sensorTask
  └─ xQueueOverwrite(SensorMessage)
          ↓
      长度为 1 的队列
          ↓
uartTask
  └─ xQueueReceive(..., portMAX_DELAY)
```

当前队列保存的是**最新状态**，不是历史数据。

长度为 1 配合 `xQueueOverwrite()` 是合理设计，但旧消息可能被新消息覆盖。README 应明确这是“最新值邮箱”语义。

### 2. 三个互斥量

| 互斥量 | 保护对象 | 主要使用位置 |
|---|---|---|
| `g_uart_mutex` | USART1 日志输出 | `Serial_SendText()` |
| `g_oled_mutex` | OLED/I2C 显示访问 | `ledTask` |
| `g_config_mutex` | `g_app_config` 配置结构体 | sensor/key/uart 相关快照与修改 |

互斥量适合保护共享资源，并带有优先级继承。

当前 `g_uart_mutex` 只保护 USART1，**没有保护 ESP8266 使用的 USART2**。

### 3. 事件标志组

当前定义的事件位：

```text
bit0  EVT_SENSOR_RUN
bit1  EVT_DISPLAY_RUN
bit2  EVT_UART_RUN
bit3  EVT_DHT11_OK
bit4  EVT_DHT11_ERR
bit5  EVT_ALARM_ACTIVE
bit6  EVT_CONFIG_SAVED
bit7  EVT_WIFI_OK
bit8  EVT_WIFI_ERR
```

`monitorTask` 等待 `SENSOR + DISPLAY + UART` 三个位全部出现，并在退出等待时自动清除它们；随后读取 WiFi、报警和配置保存状态。

### 4. 任务心跳与 IWDG

`watchdogTask` 每 3 秒读取：

```text
传感器成功次数 + 失败次数
显示任务心跳
串口任务心跳
```

只有三者都比上一次有变化时才调用：

```c
HAL_IWDG_Refresh();
```

IWDG 使用 256 分频、Reload 4095。启动后不会被无条件刷新，这一设计比“固定周期直接喂狗”更能发现任务异常。

## 六、关键问题与风险等级

### P0：ESP 与 WiFi 任务重复并发访问同一 USART2

`espTask` 和 `wifiTask` 启动后都会执行：

```text
ESP8266_BasicInit()
ESP8266_ConnectWiFi()
```

二者使用同一个 USART2 和驱动内的静态数组 `esp_rx_buf`，却没有 USART2 互斥量。

同优先级时间片切换或阻塞式接收期间，AT 命令和响应可能互相穿插。

建议：

1. 删除重复任务，合并为单一 `wifiTask` 状态机。
2. 由该任务独占 USART2 和 `esp_rx_buf`。
3. 其他任务通过 Queue 或 Task Notification 向它提交联网请求。
4. 增加断线重连、超时、退避和错误计数。

### P0：静态任务栈缓冲区与配置深度不一致

源码定义：

```c
static StackType_t wifiTaskStack[256];
static StackType_t espTaskStack[256];
```

但创建时使用：

```c
osThreadStaticDef(..., 384, wifiTaskStack, ...);
osThreadStaticDef(..., 384, espTaskStack, ...);
```

当前 `cmsis_os.c` 把 `stacksize` 直接传给 `xTaskCreateStatic()`，因此内核可能把 256 深度的数组当作 384 深度使用。

最低修复：

```c
static StackType_t wifiTaskStack[384];
static StackType_t espTaskStack[384];
```

更推荐先合并两个任务，再通过下面的接口实测后确定栈深度：

```c
uxTaskGetStackHighWaterMark(taskHandle);
```

### P1：README 任务数量与源码不一致

README 列出 7 个任务，源码创建 8 个，遗漏了 `wifiTask`。

README 中的“使用 7 个 FreeRTOS 任务”必须改为当前真实数量，或者先合并重复任务后再写 7 个。

### P1：README 中的 UART 中断接收描述不符合当前源码

README 写有：

```text
HAL_UART_Receive_IT
HAL_UART_RxCpltCallback
回调中重新启动接收
```

但当前应用源码没有这两个调用。

ESP8266 驱动使用：

```c
HAL_UART_Receive(&huart2, &ch, 1, 10);
```

这是带超时的阻塞式轮询接收。

面试时不能把未实现的中断接收当作当前项目实现。

### P1：监控窗口与采样周期不一致

`sensorTask` 允许：

```text
1000 ms <= sample_period_ms <= 10000 ms
```

但 `monitorTask` 只等待 6 秒。

如果采样周期设置为 8～10 秒，传感器和 UART 事件可能来不及在窗口内出现，导致误报。

建议：

- 将监控窗口设置为“最大采样周期 + 调度余量”，例如 12 秒。
- 或者记录各任务最后一次心跳 Tick，按照每个任务自己的周期分别判断超时。

第二种方式更加合理。

### P1：Hook 函数存在，但配置未启用

项目实现了：

```c
vApplicationMallocFailedHook();
vApplicationStackOverflowHook();
```

但应在 `FreeRTOSConfig.h` 中启用：

```c
#define configUSE_MALLOC_FAILED_HOOK    1
#define configCHECK_FOR_STACK_OVERFLOW  2
```

否则中间件默认值为 0，Hook 不会按预期工作。

### P2：周期任务全部使用相对延时

`sensorTask` 和显示任务均使用 `osDelay()`，周期包含任务本身的执行耗时，会产生漂移。

当前还配置了：

```c
#define INCLUDE_vTaskDelayUntil 0
```

如需固定采样频率，应先改为 1，再使用 `vTaskDelayUntil()`。

普通按键扫描和重试退避仍可使用相对延时。

### P2：堆与栈缺少运行数据

动态任务和 RTOS 对象共享 3072 字节堆空间，目前没有剩余堆和栈高水位记录。

建议调试时记录：

```c
xPortGetFreeHeapSize();
xPortGetMinimumEverFreeHeapSize();
uxTaskGetStackHighWaterMark(taskHandle);
```

这些测量数据比凭经验扩大栈更有说服力。

### P2：联网参数硬编码

WiFi SSID、密码以及 TCP 服务器地址直接写在源码中。

展示仓库不应提交真实凭据。建议使用占位配置、私有配置文件或 Flash 参数，并在 `.gitignore` 中排除本地秘密配置。

## 七、值得保留的设计点

1. `sensorTask` 使用长度 1 队列配合覆盖写，适合“只关心最新传感器状态”的场景。
2. `uartTask` 阻塞等待队列，不做无意义轮询。
3. USART1、OLED、配置结构体分别使用互斥量，职责比较清楚。
4. 共享传感器数据通过短临界区复制为快照，避免长时间关闭中断。
5. Event Group 同时表达任务运行、传感器、报警、配置和 WiFi 状态。
6. 看门狗根据三个关键任务心跳决定是否刷新，而不是无条件喂狗。
7. 采样周期从 Flash 配置读取，并对 1～10 秒范围进行校验。

## 八、面试标准答案

### 1. 项目有几个任务？

> 当前源码创建了 8 个应用任务，分别是 sensor、led/display、uart、key、monitor、watchdog、esp 和 wifi。其中 esp 与 wifi 职责重复，这是本次源码审计发现的问题，后续应该合并为一个联网状态机任务。README 中写 7 个任务，与当前源码不一致。

### 2. `vTaskDelay()` 和 `vTaskDelayUntil()` 有什么区别？

> `vTaskDelay()` 从调用时刻开始相对延时，任务周期会叠加本轮代码执行时间；`vTaskDelayUntil()` 以上一次计划唤醒时间为基准，更适合固定周期采样。当前工程使用 `osDelay()`，并且没有启用 `INCLUDE_vTaskDelayUntil`，这是项目可以继续优化的地方。

### 3. 队列发送的是地址还是数据？

> FreeRTOS Queue 会按照创建时指定的元素大小复制数据。本项目的队列长度为 1，元素类型为 `SensorMessage`。`sensorTask` 使用覆盖写发布最新状态，`uartTask` 阻塞接收，因此它相当于一个只保存最新数据的邮箱。

### 4. 互斥量和二值信号量有什么区别？

> 互斥量主要用于共享资源保护，具有所有权和优先级继承机制；二值信号量主要用于任务或者中断之间的事件同步，不强调所有权，也没有互斥量的优先级继承语义。本项目使用互斥量保护 USART1、OLED 和配置结构体。

### 5. 为什么 ISR 不能调用普通阻塞式 RTOS 接口？

> ISR 不能进入阻塞态等待资源，必须快速执行并退出。中断中应该调用带有 `FromISR` 后缀的接口，并根据 `higherPriorityTaskWoken` 判断是否需要在退出中断后立即触发任务切换。当前仓库还没有实现 README 中描述的 UART 中断接收链路。

### 6. 为什么看门狗不能无条件刷新？

> 如果由一个固定任务无条件喂狗，即使其他关键任务已经卡死，系统也不会复位。本项目会比较 sensor、display 和 uart 三个任务的心跳，只有三者都继续运行才喂狗。不过还需要继续优化不同任务周期下的超时判断。

### 7. 为什么队列长度设置为 1？

> 因为当前 UART 输出只关心最新一组温湿度状态，不需要保存全部历史数据。长度为 1 的队列配合 `xQueueOverwrite()` 可以减少内存占用，并保证消费者最终获得最新数据。缺点是无法记录每一次采样结果，如果需要历史数据就应该扩大队列或者增加持久化模块。

### 8. 为什么共享数据既使用队列又使用临界区？

> 队列用于把完整的传感器消息传递给 UART 任务；全局传感器结构体则会被显示和 WiFi 等多个任务读取，因此通过短临界区完成快照复制。二者解决的问题不同：队列负责消息传递，临界区负责共享内存的一致性。

### 9. 看门狗任务为什么可以设置为低优先级？

> 低优先级看门狗任务可以检测高优先级任务长期占用 CPU 的问题。如果高优先级任务发生死循环，看门狗任务无法获得调度，也就无法喂狗，最终由硬件看门狗复位系统。但必须保证正常情况下看门狗任务能够在 IWDG 超时前获得调度。

## 九、建议的修复顺序

### 本周必须完成

1. 合并 `espTask` 与 `wifiTask`，消除 USART2 并发访问。
2. 在合并前至少先修正两个静态栈缓冲区长度。
3. 修改 README 中的任务数量、任务名称、真实优先级和 UART 接收方式。
4. 调整监控超时逻辑，避免 10 秒采样周期产生误报。
5. 启用 Malloc 失败与栈溢出 Hook。

### 后续增强

1. 使用 `vTaskDelayUntil()` 实现固定周期传感器采样。
2. 增加每个任务的栈高水位、最小剩余堆和复位原因日志。
3. 将 WiFi/TCP 通信改成单任务状态机，并增加重连退避。
4. 将 WiFi 凭据和服务器地址移出公开源码。
5. 统一 `ledTask` / `displayTask` 命名。

## 十、项目真实口径

当前面试和简历可以安全表述为：

> 基于 STM32F103 和 FreeRTOS 实现环境监测系统。当前源码通过长度为 1 的消息队列将传感器最新状态发送给 UART 任务，通过互斥量保护 USART1、OLED 和配置数据，通过事件组与任务心跳监控关键任务，并由 IWDG 完成异常恢复。源码审计发现联网任务重复、README 任务数和 UART 接收方式与实现不一致，目前正在按照源码真实性进行重构。

暂时不要表述为：

- “项目只有 7 个任务”，当前源码是 8 个。
- “`watchdogTask` 优先级最高”，当前实际是 Low。
- “当前使用 UART 中断接收并在回调中重启”，当前源码未实现。
- “ESP8266 MQTT 上传已经完成”，当前代码是 AT + WiFi + TCP 框架，不是完整 MQTT 闭环。

## 十一、Day1 复盘

今天通过真实源码完成了：

- 任务数量、职责、优先级、创建方式与阻塞点审计。
- Queue、Mutex、Event Group 和 IWDG 数据流复盘。
- README 与源码差异核对。
- 联网任务并发竞争、静态栈风险、监控误报和 Hook 失效问题定位。
- 项目面试真实口径整理。

## 十二、今日必须记住的知识点

1. 单核 MCU 同一时刻只能运行一个任务。
2. 延时、等待队列和等待互斥量属于阻塞态，不是挂起态。
3. FreeRTOS Queue 默认复制数据，不只是保存地址。
4. 互斥量具有优先级继承，二值信号量主要负责同步。
5. ISR 中不能调用可能阻塞的普通 RTOS 接口。
6. `vTaskDelay()` 是相对延时，`vTaskDelayUntil()` 更适合固定周期。
7. 看门狗应该根据关键任务健康状态决定是否刷新。
8. 静态任务的栈数组长度必须与创建时传入的栈深度一致。
9. README、简历和面试表述必须与当前源码一致。
10. 项目审计不仅要看功能，还要检查并发、内存、超时和异常恢复。

## 十三、GitHub 提交建议

修复完成后建议拆分提交，不要把所有修改混在一个 Commit 中：

```text
fix: align static task stack buffers with configured depth
```

```text
refactor: merge duplicate ESP8266 networking tasks
```

```text
fix: align monitor timeout with sensor period
```

```text
config: enable FreeRTOS stack and malloc failure hooks
```

```text
docs: update FreeRTOS task architecture and UART implementation
```

本次笔记提交信息：

```text
docs: add Day1 FreeRTOS source audit notes
```

审计结束后，应优先修复 P0 问题，再更新 README，不能只修改文档来掩盖源码问题。