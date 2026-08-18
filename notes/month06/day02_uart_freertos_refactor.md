Day 2：UART与FreeRTOS并发重构
 
本次发现的问题
 
原工程同时创建 espTask 和 wifiTask，两个任务都会操作 USART2、初始化 ESP8266 并接收 AT 响应。
 
可能造成：
 
- AT 指令互相穿插
- 一个任务读走另一个任务的响应
- 静态接收缓冲区被覆盖
- 出现假超时或 HAL_BUSY
- WiFi 重复初始化
 
本次修改
 
- 删除 espTask
- 只保留 wifiTask 访问 USART2
- wifiTaskStack 从 256 改为 384
- 监控超时从 6000ms 改为 12000ms
- 补充 EVT_WIFI_OK 和 EVT_WIFI_ERR
- 保证代码中的任务数量与 README 描述一致，均为 7 个
 
核心设计思想
 
volatile 不能解决多任务并发访问外设的问题。
本项目采用单任务独占 USART2 的方式，避免多个任务争抢 ESP8266 的命令和响应。
 
当前 UART 接收方式
 
ESP8266 驱动实际使用：
 
c
  
HAL_UART_Receive(&huart2, &ch, 1, 10);