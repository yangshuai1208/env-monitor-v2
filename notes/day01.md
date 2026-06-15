# Day01 学习记录

## 今日目标

- 整理 env-monitor-v2 仓库结构
- 删除无用临时文件
- 补充 README.md 项目介绍
- 写清楚硬件组成、项目功能和 FreeRTOS 任务划分
- 完成第一次 GitHub 提交

## 今日完成

- 初步整理了 STM32 + FreeRTOS 环境监测项目仓库
- 补充了 README.md 中的项目简介、硬件组成、项目功能、软件架构
- 梳理了 FreeRTOS 中 sensor_task、oled_task、uart_task、alarm_task、storage_task 的作用
- 新增 docs/hardware.md，用于说明硬件清单和接口
- 完成 GitHub 提交

## 今日代码提交

- commit: docs: update env monitor project overview

## 实验现象

- 项目目录更加清晰
- README.md 可以完整说明项目背景和功能
- GitHub 仓库具备初步展示价值
- 后续可以继续补充系统架构图、运行说明和测试记录

## 遇到的问题

1. 现象：项目文件较多，部分文件不确定是否可以删除
2. 原因：STM32 工程会生成一些中间文件和编译缓存文件
3. 解决：只删除 Debug、build、临时文件，不删除 Core、Drivers、Middlewares 和 .ioc 文件

## 面试可讲点

- 本项目基于 STM32 + FreeRTOS 实现环境监测系统
- 使用 DHT11 采集温湿度，OLED 显示环境信息
- 使用 FreeRTOS 将采集、显示、报警、存储、串口输出拆成多个任务
- 使用模块化驱动封装 DHT11、OLED、W25Q64、EC11 等外设
- 项目具备传感器采集、数据显示、状态报警、历史存储的完整闭环

## 明日计划

- 补充 env-monitor-v2 系统架构说明
- 画出任务划分图和数据流图
- 重点整理 FreeRTOS 队列如何传递传感器数据