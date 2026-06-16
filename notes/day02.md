# Day02 学习记录

## 今日目标

- 补充 env-monitor-v2 项目系统架构说明
- 绘制 STM32 + FreeRTOS 环境监测系统架构图
- 梳理 sensor_task、oled_task、uart_task、alarm_task、storage_task 的职责
- 说明 FreeRTOS 队列如何传递温湿度数据
- 总结项目面试讲解点

## 今日完成

- 新增 docs/architecture.md，整理项目整体架构、硬件层、软件层和任务数据流
- 新增 docs/freertos_tasks.md，详细说明 FreeRTOS 任务划分
- 在 README.md 中补充系统架构和任务通信机制
- 使用 Mermaid 绘制系统架构图和任务数据流图
- 完成 GitHub 提交

## 今日代码提交

- commit: docs: add env monitor system architecture

## 实验现象

- GitHub README 中可以看到系统架构说明
- docs/architecture.md 中可以看到完整系统架构图
- docs/freertos_tasks.md 中可以看到任务职责和队列通信说明
- env-monitor-v2 项目文档更加适合简历展示和面试讲解

## 遇到的问题

1. 现象：FreeRTOS 任务较多，初期不容易讲清楚数据流
2. 原因：采集、显示、报警、存储、串口输出都依赖温湿度数据
3. 解决：将 sensor_task 作为数据源任务，通过队列将 EnvData_t 结构体传递给其他任务

## 面试可讲点

- 项目使用 FreeRTOS 将不同功能拆分为独立任务
- sensor_task 负责周期性读取 DHT11 温湿度数据
- oled_task 负责刷新 OLED 显示页面
- alarm_task 负责环境异常判断和 LED / 蜂鸣器提示
- uart_task 负责串口调试输出
- storage_task 负责将历史数据保存到 W25Q64
- 使用队列传递温湿度结构体，降低任务耦合，避免多个任务直接访问同一份全局变量

## 明日计划

- 整理 linux-iot-gateway 仓库
- 补充 Linux IoT Gateway README
- 写清楚串口接收、TCP 转发、MQTT 转发和日志模块