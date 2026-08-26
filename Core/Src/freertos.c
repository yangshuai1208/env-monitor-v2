/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"


/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "queue.h"													
#include "gpio.h"
#include "iwdg.h"
#include "usart.h"
#include "i2c.h"
#include "bsp_i2c_oled.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include "bsp_dht11.h"
#include "bsp_flash_param.h"
#include "event_groups.h"
#include "bsp_esp8266.h"
#include "sensor_data.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

								
typedef enum
{
	SET_TEMP_HIGH=0,
	SET_HUMI_LOW
}SettingItem;
	

typedef enum
{
    TASK_HEALTH_SENSOR = 0,
    TASK_HEALTH_DISPLAY,
    TASK_HEALTH_UART,
    TASK_HEALTH_COUNT
} TaskHealthId;

typedef struct
{
    TickType_t last_alive_tick;
    TickType_t deadline_ticks;
    BaseType_t seen;
} TaskHealth;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define EVT_DHT11_OK (1U<<3)        
#define EVT_DHT11_ERR (1U<<4)
#define EVT_ALARM_ACTIVE (1U<<5)
#define EVT_CONFIG_SAVED (1U<<6)


#define EVT_WIFI_OK  (1U<<7)
#define EVT_WIFI_ERR  (1U<<8)

#define WIFI_SSID  "One"
#define WIFI_PASSWORD  "123456789"





/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile SensorData g_sensor_data = {0};
QueueHandle_t g_sensor_msg_queue=NULL;										
extern volatile uint8_t g_oled_ready;
//static StaticTask_t displayTaskControlBlock;
//static StackType_t displayTaskStack[256];
//static StaticTask_t uartTaskControlBlock;
//static StackType_t uartTaskStack[256];
//static StaticTask_t sensorTaskControlBlock;
//static StackType_t sensorTaskStack[256];
/* volatile uint32_t g_display_heartbeat=0;
volatile uint32_t g_uart_heartbeat=0; */
osThreadId watchdogTaskHandle;
osThreadId monitorTaskHandle;
osThreadId keyTaskHandle;
osThreadId wifiTaskHandle;

EventGroupHandle_t g_system_event_group=NULL;

SemaphoreHandle_t g_uart_mutex=NULL;
SemaphoreHandle_t g_oled_mutex=NULL;
SemaphoreHandle_t g_config_mutex=NULL;

static StaticTask_t wifiTaskControlBlock;
static StackType_t wifiTaskStack[384];
static StaticTask_t keyTaskControlBlock;
static StackType_t keyTaskStack[256];
static StaticTask_t monitorTaskControlBlock;
static StackType_t monitorTaskStack[256];
static StaticTask_t watchdogTaskControlBlock;
static StackType_t watchdogTaskStack[256];
extern AppConfig g_app_config;
volatile SettingItem g_setting_item=SET_TEMP_HIGH;
volatile uint8_t g_config_changed=0;



static TaskHealth g_task_health[TASK_HEALTH_COUNT] =
{
    {0U, pdMS_TO_TICKS(15000U), pdFALSE},
    {0U, pdMS_TO_TICKS(4000U),  pdFALSE},
    {0U, pdMS_TO_TICKS(15000U), pdFALSE}
};
/* USER CODE END Variables */
osThreadId ledTaskHandle;
osThreadId uartTaskHandle;
osThreadId sensorTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void Serial_SendText(const char *text);

void StartWatchdogTask(void const* argument);
static BaseType_t AppConfig_GetSnapshot(AppConfig *config);
void StartMonitorTask(void const*argument);
void StartKeyTask(void const*argument);
void  StartWiFiTask(void const*argument);

static void TaskHealth_Report(TaskHealthId id);

static uint32_t TaskHealth_CollectExpiredMask(
    TickType_t now_tick
);
/* USER CODE END FunctionPrototypes */

void StartLEDTask(void const * argument);
void StartUartTask(void const * argument);
void StartSensorTask(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

static void Serial_SendText(const char *text)
{
  if (text == NULL)
  {
    return;
  }
	if(g_uart_mutex!=NULL)
	{
		if(xSemaphoreTake(g_uart_mutex,pdMS_TO_TICKS(200))==pdPASS)
		{
			HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
			xSemaphoreGive(g_uart_mutex);
		}
	}
	else
	{
				HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100);
	}
}

static BaseType_t AppConfig_GetSnapshot(AppConfig *config)
{
    if ((config == NULL) || (g_config_mutex == NULL))
    {
        return pdFAIL;
    }

    if (xSemaphoreTake(g_config_mutex, pdMS_TO_TICKS(200U)) != pdPASS)
    {
        return pdFAIL;
    }

    *config = g_app_config;

    xSemaphoreGive(g_config_mutex);
    return pdPASS;
}

/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
	g_sensor_msg_queue = xQueueCreate(1U, sizeof(SensorData));					
	if(g_sensor_msg_queue==NULL)
	{
		Error_Handler();
	
	}
	g_uart_mutex=xSemaphoreCreateMutex();
	if(g_uart_mutex==NULL)
	{
		Error_Handler();
	}
	g_oled_mutex=xSemaphoreCreateMutex();
	if(g_oled_mutex==NULL)
	{
		Error_Handler();
	}
	g_config_mutex=xSemaphoreCreateMutex();
	if(g_config_mutex==NULL)
	{
		Error_Handler();
	}
	g_system_event_group=xEventGroupCreate();
	if(g_system_event_group==NULL)
	{
		Error_Handler();
	}
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of ledTask */
  osThreadDef(ledTask, StartLEDTask, osPriorityNormal, 0, 256);
  ledTaskHandle = osThreadCreate(osThread(ledTask), NULL);

  /* definition and creation of uartTask */
  osThreadDef(uartTask, StartUartTask, osPriorityLow, 0, 128);
  uartTaskHandle = osThreadCreate(osThread(uartTask), NULL);

  /* definition and creation of sensorTask */
  osThreadDef(sensorTask, StartSensorTask, osPriorityAboveNormal, 0, 128);
  sensorTaskHandle = osThreadCreate(osThread(sensorTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
	osThreadStaticDef(watchdogTask,StartWatchdogTask,osPriorityLow,0,256,watchdogTaskStack,&watchdogTaskControlBlock);
	watchdogTaskHandle=osThreadCreate(osThread(watchdogTask),NULL);
	if(watchdogTaskHandle==NULL)
	{
		Error_Handler();
		
	}
	osThreadStaticDef(keyTask,StartKeyTask,osPriorityNormal,0,256,keyTaskStack,&keyTaskControlBlock);
	keyTaskHandle=osThreadCreate(osThread(keyTask),NULL);
	if(keyTaskHandle==NULL)
	{
		Error_Handler();
		
	}
	osThreadStaticDef(monitorTask,StartMonitorTask,osPriorityLow,0,256,monitorTaskStack,&monitorTaskControlBlock);
	monitorTaskHandle=osThreadCreate(osThread(monitorTask),NULL);
	if(monitorTaskHandle==NULL)
	{
		Error_Handler();
		
	}
	osThreadStaticDef(wifiTask,StartWiFiTask,osPriorityLow,0,384,wifiTaskStack,&wifiTaskControlBlock);
	wifiTaskHandle=osThreadCreate(osThread(wifiTask),NULL);
	if(wifiTaskHandle==NULL)
	{
		Error_Handler();
		
	}
	
	
	
	
	
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartLEDTask */
/**
* @brief Function implementing the ledTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLEDTask */
void StartLEDTask(void const * argument)
{
  /* USER CODE BEGIN StartLEDTask */
	 char temp_buf[20];
  char humi_buf[20];
  char cnt_buf[20];
  char err_buf[20];
	char alarm_buf[20];
  SensorData sensor_snapshot;
  (void)argument;																			//�������argument������ûʹ��
  /* Infinite loop */
  for(;;)
    {
       taskENTER_CRITICAL();
      sensor_snapshot = g_sensor_data;
      taskEXIT_CRITICAL();

//        HAL_GPIO_TogglePin(GPIOA, LED_R_Pin);
//        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);

        if (g_oled_ready)
        {
						if(xSemaphoreTake(g_oled_mutex,pdMS_TO_TICKS(200))==pdPASS)
						{	
							OLED_CLS();																				//����
            if (sensor_snapshot.ready == 0U)
            {
                OLED_ShowString_F8X16(0, 0, (uint8_t *)"DHT11");
                OLED_ShowString_F8X16(1, 0, (uint8_t *)"WAIT DATA");
            }
            else if (sensor_snapshot.valid)
            {
                sprintf(temp_buf, "TEMP:%d.%dC",
                        sensor_snapshot.temp_int,
                        sensor_snapshot.temp_dec);
                sprintf(humi_buf, "HUMI:%d.%d%%",
                        sensor_snapshot.humi_int,
                        sensor_snapshot.humi_dec);
							sprintf(cnt_buf,"CNT:%lu",
												sensor_snapshot.update_count);
							sprintf(err_buf,"ERR:%lu",
												sensor_snapshot.error_count);
						switch(sensor_snapshot.alarm_state)
						{
							case ALARM_NONE:
							sprintf(alarm_buf,"NORMAL");
							break;
							
							case ALARM_TEMP_HIGH:
							sprintf(alarm_buf,"TEMP HIGH");
							break;
							
							case ALARM_HUMI_LOW:
							sprintf(alarm_buf,"HUMI LOW");
							break;
							
							case ALARM_BOTH:
							sprintf(alarm_buf,"ALARM_BOTH");
							break;
							
							default:
							sprintf(alarm_buf,"UNKNOWN");
							break;
						}	
							
				
                OLED_ShowString_F8X16(0, 0, (uint8_t *)"DHT11");
                OLED_ShowString_F8X16(1, 0, (uint8_t *)temp_buf);
                OLED_ShowString_F8X16(2, 0, (uint8_t *)humi_buf);
								OLED_ShowString_F8X16(3,0,(uint8_t*)alarm_buf);
            }
				
            else
            {			
								sprintf(err_buf,"ERR:%lu",sensor_snapshot.error_count);
                OLED_ShowString_F8X16(0, 0, (uint8_t *)"DHT11");
                OLED_ShowString_F8X16(1, 0, (uint8_t *)"READ ERROR");
								OLED_ShowString_F8X16(3,0,(uint8_t*)err_buf);
            }
								xSemaphoreGive(g_oled_mutex);
        }
        else
        {
            HAL_GPIO_TogglePin(GPIOA, LED_R_Pin);
        }
			}
				
		TaskHealth_Report(TASK_HEALTH_DISPLAY);
        osDelay(1000U);										
    }
  /* USER CODE END StartLEDTask */
}

/* USER CODE BEGIN Header_StartUartTask */
/**
* @brief Function implementing the uartTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUartTask */
void StartUartTask(void const * argument)
{
  /* USER CODE BEGIN StartUartTask */
	  char uart_buf[128];
  SensorData msg;
  SensorData_Init(&msg);
  (void)argument;
	AppConfig config_snapshot;
	
  /* Infinite loop */
   
    for(;;)
    {
				
			  if(xQueueReceive(g_sensor_msg_queue,&msg,portMAX_DELAY)==pdPASS)				
				{		
						if (AppConfig_GetSnapshot(&config_snapshot) != pdPASS)
          {
             Serial_SendText("CONFIG SNAPSHOT ERROR\r\n");
              continue;
          }
						HAL_GPIO_TogglePin(GPIOA, LED_R_Pin);
//						HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
				
				
        if (msg.valid)
        {
          
					sprintf(uart_buf, "DHT11 TEMP:%d.%dC HUMI:%d.%d%% CNT:%lu ERR:%lu ALARM:%s TH:%d HL:%d\r\n",
              msg.temp_int,
              msg.temp_dec,
              msg.humi_int,
              msg.humi_dec,
							msg.update_count,
							msg.error_count,
						 SensorData_AlarmToString(msg.alarm_state),	
							config_snapshot.temp_alarm_high,
							config_snapshot.humi_alarm_low);
				}					
       else
       {
							sprintf(uart_buf, "DHT11 READ ERROR  CNT:%lu ERR:%lu \r\n",
							msg.update_count,
							msg.error_count);
       }
            Serial_SendText(uart_buf);
        TaskHealth_Report(TASK_HEALTH_UART);
       }
     
    }

  
  /* USER CODE END StartUartTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the sensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void const * argument)
{
  /* USER CODE BEGIN StartSensorTask */
DHT11_DATA_TYPEDEF dht11_data;
SensorData msg;
AppConfig config_snapshot;
uint8_t dht11_ok = 0U;

SensorData_Init(&msg);
	
  (void)argument;
  osDelay(1000);															//�ȶ�
	
  /* Infinite loop */
  for(;;)
  {
				if (AppConfig_GetSnapshot(&config_snapshot) != pdPASS)
        {
            Serial_SendText("CONFIG SNAPSHOT ERROR\r\n");
            osDelay(100U);
             continue;
        }
       if (DHT11_ReadData(&dht11_data) == HAL_OK)
      {
      dht11_ok = 1U;

      taskENTER_CRITICAL();

    g_sensor_data.temp_int = dht11_data.temp_int;
    g_sensor_data.temp_dec = dht11_data.temp_deci;
    g_sensor_data.humi_int = dht11_data.humi_int;
    g_sensor_data.humi_dec = dht11_data.humi_deci;
    g_sensor_data.ready = 1U;
    g_sensor_data.valid = 1U;
    g_sensor_data.update_count++;

    g_sensor_data.alarm_state = SensorData_CheckAlarm(
        g_sensor_data.temp_int,
        g_sensor_data.humi_int,
        config_snapshot.temp_alarm_high,
        config_snapshot.humi_alarm_low
    );

    msg = g_sensor_data;

    taskEXIT_CRITICAL();
}
else
{
    dht11_ok = 0U;

    taskENTER_CRITICAL();

    g_sensor_data.ready = 1U;
    g_sensor_data.valid = 0U;
    g_sensor_data.error_count++;
    g_sensor_data.alarm_state = ALARM_NONE;

    msg = g_sensor_data;

    taskEXIT_CRITICAL();
}
				if (g_system_event_group != NULL)
				{
					
						if (dht11_ok)
						{
								xEventGroupSetBits(g_system_event_group, EVT_DHT11_OK);
								xEventGroupClearBits(g_system_event_group, EVT_DHT11_ERR);
						}
						else
						{
								xEventGroupSetBits(g_system_event_group, EVT_DHT11_ERR);
								xEventGroupClearBits(g_system_event_group, EVT_DHT11_OK);
						}

						if (msg.alarm_state != ALARM_NONE)
						{
								xEventGroupSetBits(g_system_event_group, EVT_ALARM_ACTIVE);
						}
						else
						{
								xEventGroupClearBits(g_system_event_group, EVT_ALARM_ACTIVE);
						}
				}
				xQueueOverwrite(g_sensor_msg_queue,&msg);		
                TaskHealth_Report(TASK_HEALTH_SENSOR);	
        uint16_t period=config_snapshot.sample_period_ms;   

				if(period<1000||period>10000)
				{
						period=2000;
				}
				osDelay(period);
  }
  /* USER CODE END StartSensorTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

#define KEY_DEBOUNCE_MS 20U

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;

    GPIO_PinState raw_state;       
    GPIO_PinState stable_state;    
    TickType_t change_tick;       
} KeyDebounce;

static void KeyDebounce_Init(KeyDebounce *key,
                             GPIO_TypeDef *port,
                             uint16_t pin)
{
    GPIO_PinState current_state;

    current_state = HAL_GPIO_ReadPin(port, pin);

    key->port = port;
    key->pin = pin;
    key->raw_state = current_state;
    key->stable_state = current_state;
    key->change_tick = xTaskGetTickCount();
}

static uint8_t KeyDebounce_Poll(KeyDebounce *key)
{
    GPIO_PinState current_state;
    TickType_t current_tick;

    current_state = HAL_GPIO_ReadPin(key->port, key->pin);
    current_tick = xTaskGetTickCount();

   
    if (current_state != key->raw_state)
    {
        key->raw_state = current_state;
        key->change_tick = current_tick;
    }

    
    if ((current_state != key->stable_state) &&
        ((current_tick - key->change_tick) >=
         pdMS_TO_TICKS(KEY_DEBOUNCE_MS)))
    {
        key->stable_state = current_state;

      
        if (current_state == GPIO_PIN_RESET)
        {
            return 1U;
        }
    }

    return 0U;
}

static void TaskHealth_Report(TaskHealthId id)
{
    TickType_t now_tick;

    if ((uint32_t)id >=
        (uint32_t)TASK_HEALTH_COUNT)
    {
        return;
    }

    now_tick = xTaskGetTickCount();

    taskENTER_CRITICAL();

    g_task_health[id].last_alive_tick =
        now_tick;

    g_task_health[id].seen = pdTRUE;

    taskEXIT_CRITICAL();
}

static uint32_t TaskHealth_CollectExpiredMask(
    TickType_t now_tick)
{
    TaskHealth snapshot[TASK_HEALTH_COUNT];
    uint32_t mask = 0U;

    /*
     * 临界区内只复制数据，
     * 超时计算放在临界区外完成。
     */
    taskENTER_CRITICAL();

    memcpy(
        snapshot,
        g_task_health,
        sizeof(snapshot)
    );

    taskEXIT_CRITICAL();

    for (uint32_t index = 0U;
         index < (uint32_t)TASK_HEALTH_COUNT;
         ++index)
    {
        TickType_t elapsed =
            now_tick -
            snapshot[index].last_alive_tick;

        if ((snapshot[index].seen == pdFALSE) ||
            (elapsed >
             snapshot[index].deadline_ticks))
        {
            mask |= (1UL << index);
        }
    }

    return mask;
}
void StartWatchdogTask(void const *argument)
{
    uint32_t expired_mask;

    (void)argument;

    /*
     * 给三个被监控任务启动时间。
     */
    osDelay(3000U);

    for (;;)
    {
        expired_mask =
            TaskHealth_CollectExpiredMask(
                xTaskGetTickCount()
            );

        if (expired_mask == 0U)
        {
            (void)HAL_IWDG_Refresh(&hiwdg);
        }

        /*
         * 存在超时任务时停止喂狗，
         * 等待硬件看门狗复位。
         */
        osDelay(3000U);
    }
}
void StartKeyTask(void const * argument)
{
    (void)argument;
KeyDebounce mode_key;
KeyDebounce up_key;
KeyDebounce down_key;

KeyDebounce_Init(&mode_key,
                 KEY_MODE_GPIO_Port,
                 KEY_MODE_Pin);

KeyDebounce_Init(&up_key,
                 KEY_UP_GPIO_Port,
                 KEY_UP_Pin);

KeyDebounce_Init(&down_key,
                 KEY_DOWN_GPIO_Port,
                 KEY_DOWN_Pin);
    for (;;)
    {
        if (KeyDebounce_Poll(&mode_key))
        {
            if (g_setting_item == SET_TEMP_HIGH)
            {
                g_setting_item = SET_HUMI_LOW;
            }
            else
            {
                g_setting_item = SET_TEMP_HIGH;
            }

            Serial_SendText("KEY MODE\r\n");
        }

        if (KeyDebounce_Poll(&up_key))
        {
					if(xSemaphoreTake(g_config_mutex,pdMS_TO_TICKS(500))==pdPASS)
					{	
            if (g_setting_item == SET_TEMP_HIGH)
            {
                g_app_config.temp_alarm_high++;
            }
            else
            {
                g_app_config.humi_alarm_low++;
            }
						g_config_changed=1;
            xSemaphoreGive(g_config_mutex);
          }
						 Serial_SendText("KEY UP\r\n");
        }

        if (KeyDebounce_Poll(&down_key))
        {
					if(xSemaphoreTake(g_config_mutex,pdMS_TO_TICKS(500))==pdPASS)
					{
            if (g_setting_item == SET_TEMP_HIGH)
            {
                if (g_app_config.temp_alarm_high > 0)
                {
                    g_app_config.temp_alarm_high--;
                }
            }
            else
            {
                if (g_app_config.humi_alarm_low > 0)
                {
                    g_app_config.humi_alarm_low--;
                }
            }

            g_config_changed = 1;
						xSemaphoreGive(g_config_mutex);
					}
            Serial_SendText("KEY DOWN\r\n");
        }
				if(g_config_changed)
				{
					AppConfig config_to_save;
					
					if (xSemaphoreTake(g_config_mutex,pdMS_TO_TICKS(500))==pdPASS)
					{
						config_to_save=g_app_config;
						g_config_changed=0;
						xSemaphoreGive(g_config_mutex);
            if (AppConfig_Save(&config_to_save) == 0)
            {
                Serial_SendText("CONFIG SAVED\r\n");
									if(g_system_event_group!=NULL)
						{
						xEventGroupSetBits(g_system_event_group,EVT_CONFIG_SAVED);
					
						}
            }
            else
            {
                Serial_SendText("CONFIG SAVE ERROR\r\n");
            }
					}
				}
        osDelay(10);
    }
}


void vApplicationMallocFailedHook(void)
{
  vAssertCalled("malloc", 0);
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  (void)xTask;
  (void)pcTaskName;
  vAssertCalled("stack", 0);
}

void vAssertCalled(const char *file, int line)
{
  (void)file;
  (void)line;
  taskDISABLE_INTERRUPTS();

  while (1)
  {
    HAL_GPIO_TogglePin(GPIOA, LED_R_Pin );
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    for (volatile uint32_t delay = 0; delay < 250000U; delay++)
    {
      __NOP();
    }
  }
}
void StartMonitorTask(void const *argument)
{
    EventBits_t now_bits;
    EventBits_t previous_status_bits = 0U;
    EventBits_t changed_bits;

    uint32_t expired_mask;
    uint32_t previous_expired_mask =
        UINT32_MAX;

    const EventBits_t tracked_status_bits =
        EVT_DHT11_OK |
        EVT_DHT11_ERR |
        EVT_WIFI_OK |
        EVT_WIFI_ERR |
        EVT_ALARM_ACTIVE;

    (void)argument;

    osDelay(3000U);

    for (;;)
    {
        /*
         * 一、检查任务存活状态
         */
        expired_mask =
            TaskHealth_CollectExpiredMask(
                xTaskGetTickCount()
            );

        /*
         * 只在状态变化时打印，
         * 避免每秒重复刷屏。
         */
        if (expired_mask !=
            previous_expired_mask)
        {
            if (expired_mask == 0U)
            {
                Serial_SendText(
                    "SYS TASKS OK\r\n"
                );
            }
            else
            {
                if ((expired_mask &
                     (1UL <<
                      TASK_HEALTH_SENSOR)) != 0U)
                {
                    Serial_SendText(
                        "SYS SENSOR TASK TIMEOUT\r\n"
                    );
                }

                if ((expired_mask &
                     (1UL <<
                      TASK_HEALTH_DISPLAY)) != 0U)
                {
                    Serial_SendText(
                        "SYS DISPLAY TASK TIMEOUT\r\n"
                    );
                }

                if ((expired_mask &
                     (1UL <<
                      TASK_HEALTH_UART)) != 0U)
                {
                    Serial_SendText(
                        "SYS UART TASK TIMEOUT\r\n"
                    );
                }
            }

            previous_expired_mask =
                expired_mask;
        }

        /*
         * 二、读取业务状态
         */
        now_bits =
            xEventGroupGetBits(
                g_system_event_group
            );

        /*
         * XOR得到发生变化的位。
         */
        changed_bits =
            (now_bits ^
             previous_status_bits) &
            tracked_status_bits;

        if (((changed_bits &
              EVT_DHT11_OK) != 0U) &&
            ((now_bits &
              EVT_DHT11_OK) != 0U))
        {
            Serial_SendText(
                "SYS DHT11 DATA OK\r\n"
            );
        }

        if (((changed_bits &
              EVT_DHT11_ERR) != 0U) &&
            ((now_bits &
              EVT_DHT11_ERR) != 0U))
        {
            Serial_SendText(
                "SYS DHT11 DATA ERROR\r\n"
            );
        }

        if (((changed_bits &
              EVT_WIFI_OK) != 0U) &&
            ((now_bits &
              EVT_WIFI_OK) != 0U))
        {
            Serial_SendText(
                "SYS WIFI OK\r\n"
            );
        }

        if (((changed_bits &
              EVT_WIFI_ERR) != 0U) &&
            ((now_bits &
              EVT_WIFI_ERR) != 0U))
        {
            Serial_SendText(
                "SYS WIFI ERROR\r\n"
            );
        }

        if ((changed_bits &
             EVT_ALARM_ACTIVE) != 0U)
        {
            if ((now_bits &
                 EVT_ALARM_ACTIVE) != 0U)
            {
                Serial_SendText(
                    "SYS ALARM ACTIVE\r\n"
                );
            }
            else
            {
                Serial_SendText(
                    "SYS ALARM CLEARED\r\n"
                );
            }
        }

        /*
         * CONFIG_SAVED是一次性事件。
         */
        if ((now_bits &
             EVT_CONFIG_SAVED) != 0U)
        {
            Serial_SendText(
                "SYS CONFIG SAVED\r\n"
            );

            xEventGroupClearBits(
                g_system_event_group,
                EVT_CONFIG_SAVED
            );
        }

        previous_status_bits =
            now_bits &
            tracked_status_bits;

        osDelay(1000U);
    }
}
void StartWiFiTask(void const *argument)
{
    char buf[64];
    uint8_t ret;

    (void)argument;
    osDelay(5000);

    Serial_SendText("ESP8266 TASK START\r\n");

    /* 1. ESP8266 基础初始化 */
    ret = ESP8266_BasicInit();

    if (ret != 0U)
    {
        Serial_SendText("ESP8266 AT ERROR\r\n");

        if (g_system_event_group != NULL)
        {
            xEventGroupClearBits(g_system_event_group, EVT_WIFI_OK);
            xEventGroupSetBits(g_system_event_group, EVT_WIFI_ERR);
        }

        for (;;)
        {
            osDelay(10000);
        }
    }

    Serial_SendText("ESP8266 AT OK\r\n");

    /* 2. 连接 WiFi */
    ret = ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASSWORD);

    if (ret != 0U)
    {
        Serial_SendText("ESP8266 WIFI CONNECT FAIL\r\n");

        if (g_system_event_group != NULL)
        {
            xEventGroupClearBits(g_system_event_group, EVT_WIFI_OK);
            xEventGroupSetBits(g_system_event_group, EVT_WIFI_ERR);
        }

        for (;;)
        {
            osDelay(10000);
        }
    }

    Serial_SendText("ESP8266 WIFI CONNECT OK\r\n");

    if (g_system_event_group != NULL)
    {
        xEventGroupClearBits(g_system_event_group, EVT_WIFI_ERR);
        xEventGroupSetBits(g_system_event_group, EVT_WIFI_OK);
    }

    /* 3. 连接 TCP 服务器 */
    if (ESP8266_TCPConnect("192.168.1.100", 12345) != 0U)
    {
        Serial_SendText("TCP CONNECT FAIL\r\n");

        /*
         * TCP 失败不代表 WiFi 已断开，
         * 因此这里不设置 EVT_WIFI_ERR。
         */
        for (;;)
        {
            osDelay(10000);
        }
    }

    Serial_SendText("TCP CONNECT OK\r\n");

    /* 4. 周期上传传感器数据 */
    for (;;)
    {
        SensorData snapshot;

        taskENTER_CRITICAL();
        snapshot = g_sensor_data;
        taskEXIT_CRITICAL();

        if (snapshot.valid != 0U)
        {
            int len = snprintf(
                buf,
                sizeof(buf),
                "TEMP=%d.%d,HUMI=%d.%d\r\n",
                snapshot.temp_int,
                snapshot.temp_dec,
                snapshot.humi_int,
                snapshot.humi_dec
            );

            if ((len > 0) && (len < (int)sizeof(buf)))
            {
                if (ESP8266_TCPSend(
                        (uint8_t *)buf,
                        (uint16_t)len
                    ) == 0U)
                {
                    Serial_SendText("TCP SEND OK\r\n");
                }
                else
                {
                    Serial_SendText("TCP SEND FAIL\r\n");
                }
            }
            else
            {
                Serial_SendText("TCP FORMAT ERROR\r\n");
            }
        }

        osDelay(5000);
    }
}

/* USER CODE END Application */