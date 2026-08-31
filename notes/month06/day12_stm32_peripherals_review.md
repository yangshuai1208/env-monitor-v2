# Day12：STM32通信外设与UART环形缓冲区复习

## 一、当天完成内容

- 复习UART 115200、8N1及理论传输速率。
- 复习UART轮询、中断、DMA接收方式。
- 复习I²C、SPI、PWM、ADC、DMA高频知识。
- 复习DHT11、OLED、W25Q64、PCA9685和MG90S排错流程。
- 编写主机端UART环形缓冲区及边界测试。

## 二、UART

115200、8N1发送一个字节需要：

- 1位起始位。
- 8位数据位。
- 1位停止位。

因此理论有效速率约为11520字节/秒。

`HAL_UART_Receive_IT()`的一次调用只启动一次接收，接收完成后需要在`HAL_UART_RxCpltCallback()`中重新启动。

中断回调只负责保存字节、更新索引和通知任务，复杂的组帧、校验和命令解析应放到任务中。

## 三、I²C与SPI

I²C使用SDA和SCL，开漏输出需要上拉电阻。STM32 HAL通常要求将七位地址左移一位。

SPI使用SCK、MOSI、MISO和CS。W25Q64写入前需要Write Enable，写入后通过状态寄存器BUSY位等待操作完成。

## 四、PWM、ADC与DMA

PWM频率：

f_pwm = f_timer / ((PSC + 1) × (ARR + 1))

72MHz定时器、PSC=71、ARR=19999时，计数频率为1MHz，PWM频率为50Hz。1.5ms舵机脉宽对应CCR=1500。

12位ADC范围为0～4095：

V = ADC_raw / 4095 × Vref

DMA可以降低CPU搬运数据的开销，但不能自动解决UART粘包、半包和协议边界。

## 五、UART环形缓冲区

新增文件：

`host_tests/day12_uart_ring_buffer.c`

实现：

- 缓冲区初始化。
- FIFO字节写入和读取。
- 满缓冲区拒绝覆盖旧数据。
- 空缓冲区不修改输出参数。
- head和tail回绕。
- 空指针检查。

测试覆盖初始化、FIFO、满、空、回绕和非法参数。

当前代码为主机端练习和测试实现，尚未宣称已经集成到STM32板载固件。

## 六、调试问题

- 非void函数成功路径必须返回值。
- `=`是赋值，`==`才是比较。
- 无符号计数器从0减1会回绕到最大值。
- 中断和任务共享`count`时存在竞态，`volatile`不能代替同步。
- 实际板载版本应使用临界区或单生产者单消费者设计。

## 七、编译验证

编译：

`gcc -std=c11 -Wall -Wextra -Wpedantic -Werror host_tests/day12_uart_ring_buffer.c -o build/day12_uart_ring_buffer`

运行：

`./build/day12_uart_ring_buffer`

实际结果：

`UART ring buffer tests passed`

## 八、延期内容

以下内容顺延至Day13下午：

- 位运算与整数溢出错题复盘。
- IWDG与MQTT项目追问。
- Day11最后一道工程代码题。