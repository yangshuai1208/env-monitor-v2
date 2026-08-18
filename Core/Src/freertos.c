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
#include "bsp_dht11.h"
#include "bsp_flash_param.h"
#include "event_groups.h"
#include "bsp_esp8266.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
	typedef enum
	{		
		ALARM_NONE=0,
		ALARM_TEMP_HIGH,
		ALARM_HUMI_LOW,
		ALARM_BOTH
	}AlarmState;
	
	typedef struct
	{
			uint8_t temp_int;
			uint8_t temp_dec;
			uint8_t humi_int;
			uint8_t humi_dec;
			uint8_t ready;
			uint8_t valid;
			uint32_t update_count;						
			uint32_t error_count;	
			AlarmState alarm_state;
	} SensorData;

		typedef struct
	{
			uint8_t temp_int;
			uint8_t temp_dec;
			uint8_t humi_int;
			uint8_t humi_dec;
			uint8_t valid;
			uint32_t update_count;							
			uint32_t error_count;
			AlarmState alarm_state;
	} SensorMessage;										
typedef enum
{
	SET_TEMP_HIGH=0,
	SET_HUMI_LOW
}SettingItem;
	
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define EVT_SENSOR_RUN (1U<<0)
#define EVT_DISPLAY_RUN (1U<<1)
#define EVT_UART_RUN (1U<<2)
#define EVT_DHT11_OK (1U<<3)
#define EVT_DHT11_ERR (1U<<4)
#define EVT_ALARM_ACTIVE (1U<<5)
#define EVT_CONFIG_SAVED (1U<<6)
#define MONITOR_TASK_TIMEOUT_MS 12000U

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
volatile uint32_t g_display_heartbeat=0;
volatile uint32_t g_uart_heartbeat=0;
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
/* USER CODE END Variables */
osThreadId ledTaskHandle;
osThreadId uartTaskHandle;
osThreadId sensorTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void Serial_SendText(const char *text);
static AlarmState CheckAlarmState(uint8_t temp_int,
																	uint8_t humi_int,
																	int16_t temp_alarm_high,
																	int16_t humi_alarm_low);
void StartWatchdogTask(void const* argument);
static void AppConfig_GetSnapshot(AppConfig*config);
void StartMonitorTask(void const*argument);
void StartKeyTask(void const*argument);
void  StartWiFiTask(void const*argument);


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
static AlarmState CheckAlarmState(uint8_t temp_int,
																	uint8_t humi_int,
																	int16_t temp_alarm_high,
																	int16_t humi_alarm_low)
{											
	uint8_t temp_high=0;
	uint8_t humi_low=0;
	if(temp_int>=temp_alarm_high)
	{
		temp_high=1;
	}
	if(humi_int<=humi_alarm_low)
	{
		humi_low=1;
	}
	if(temp_high&&humi_low)
	{
		return ALARM_BOTH;
	}
	else if(temp_high)
	{
		return ALARM_TEMP_HIGH;
	}
	else if(humi_low)
	{
		return ALARM_HUMI_LOW;
	}
	else 
	{	
		return ALARM_NONE;
	}
}
static void AppConfig_GetSnapshot(AppConfig*config)
{
	if(config==NULL)
	{
		return;
	}
	if(g_config_mutex!=NULL)
	{
		if(xSemaphoreTake(g_config_mutex,pdMS_TO_TICKS(200))==pdPASS)
		{	
			*config=g_app_config;
			xSemaphoreGive(g_config_mutex);
		}
	}
	else
	{
		*config=g_app_config;
	}
}

static const char *AlarmStateToString(AlarmState state)
{
							switch(state)
						{
							case ALARM_NONE:
							return "NORMAL";
							
							
							case ALARM_TEMP_HIGH:
							return "TEMP_HIGH";
							
							case ALARM_HUMI_LOW:
							return  "HUMI_LOW";
			
							
							case ALARM_BOTH:
							return "TEMP_HIGH_HUMI_LOW";
					
							
							default:
							return "UNKNOWN";
						
						}	
}
static uint8_t Key_IsPressed(GPIO_TypeDef*GPIOx,uint16_t GPIO_Pin)
{
	if(HAL_GPIO_ReadPin(GPIOx,GPIO_Pin)==GPIO_PIN_RESET)
	{
		osDelay(20);
	if(HAL_GPIO_ReadPin(GPIOx,GPIO_Pin)==GPIO_PIN_RESET)
	{
		while(HAL_GPIO_ReadPin(GPIOx,GPIO_Pin)==GPIO_PIN_RESET)
		{
			osDelay(10);
		}
		return 1;
	}
}
	return 0;
}	
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
	g_sensor_msg_queue=xQueueCreate(1,sizeof(SensorMessage));						//ֻ��������һ֡������״̬����������ʷ��Ϣ������ UART ������������ݡ�
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
        taskENTER_CRITICAL();													//�����ٽ���
        sensor_snapshot.temp_int = g_sensor_data.temp_int;				//�ѹ������ݿ��������ؿ����
        sensor_snapshot.temp_dec = g_sensor_data.temp_dec;
        sensor_snapshot.humi_int = g_sensor_data.humi_int;
        sensor_snapshot.humi_dec = g_sensor_data.humi_dec;
        sensor_snapshot.ready = g_sensor_data.ready;						//�Ƿ�׼����
        sensor_snapshot.valid = g_sensor_data.valid;						//��ǰ�����Ƿ���Ч
				sensor_snapshot.update_count=g_sensor_data.update_count;
				sensor_snapshot.error_count=g_sensor_data.error_count;
				sensor_snapshot.alarm_state=g_sensor_data.alarm_state;
        taskEXIT_CRITICAL();																		//�˳��ٽ���

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
					if(g_system_event_group!=NULL)
					{
						xEventGroupSetBits(g_system_event_group,EVT_DISPLAY_RUN);
					
					}
				g_display_heartbeat++;
        osDelay(1000);																	//OLEDÿһ��ˢ��һ��
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
  SensorMessage msg;
  (void)argument;
	AppConfig config_snapshot;
	
  /* Infinite loop */
   
    for(;;)
    {
				
			  if(xQueueReceive(g_sensor_msg_queue,&msg,portMAX_DELAY)==pdPASS)					//��g_sensor_msg_queueȡһ����Ϣ���Ž�msg�������������ʱû����Ϣ��һֱ��
				{		
						AppConfig_GetSnapshot(&config_snapshot);
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
						  AlarmStateToString(msg.alarm_state),		
							config_snapshot.temp_alarm_high,
							config_snapshot.humi_alarm_low);
				}					
       else
       {
							sprintf(uart_buf, "DHT11 READ ERROR  CNT:%lu ERR:%lu \r\n",
							msg.update_count,
							msg.error_count);
       }
						if(g_system_event_group!=NULL)
					{
						xEventGroupSetBits(g_system_event_group,EVT_UART_RUN);
					
					}
          Serial_SendText(uart_buf);
					g_uart_heartbeat++;
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
  SensorMessage msg;
	AppConfig config_snapshot;
	uint8_t dht11_ok = 0;
	
  (void)argument;
  osDelay(1000);															//�ȶ�
	
  /* Infinite loop */
  for(;;)
  {
				AppConfig_GetSnapshot(&config_snapshot);
        if (DHT11_ReadData(&dht11_data) == HAL_OK)				//��ȡ�ɹ�������HAL_OK
        {
						dht11_ok = 1;
            taskENTER_CRITICAL();
            g_sensor_data.temp_int = dht11_data.temp_int;
            g_sensor_data.temp_dec = dht11_data.temp_deci;
            g_sensor_data.humi_int = dht11_data.humi_int;
            g_sensor_data.humi_dec = dht11_data.humi_deci;
            g_sensor_data.ready = 1;
            g_sensor_data.valid = 1;
						g_sensor_data.update_count++;
						g_sensor_data.alarm_state=CheckAlarmState(g_sensor_data.temp_int,
																											g_sensor_data.humi_int,
																											config_snapshot.temp_alarm_high,
																											config_snapshot.humi_alarm_low);
						msg.temp_int=g_sensor_data.temp_int;
						msg.temp_dec=g_sensor_data.temp_dec;
						msg.humi_int=g_sensor_data.humi_int;
						msg.humi_dec=g_sensor_data.humi_dec;
						msg.valid=1;
						msg.update_count=g_sensor_data.update_count;
						msg.error_count=g_sensor_data.error_count;
						msg.alarm_state=g_sensor_data.alarm_state;
					
						taskEXIT_CRITICAL();
				
        }
        else
        {
						dht11_ok = 0;
            taskENTER_CRITICAL();
            g_sensor_data.ready = 1;
            g_sensor_data.valid = 0;
						g_sensor_data.error_count++;
            g_sensor_data.alarm_state=ALARM_NONE;
					
					
						msg.temp_int=g_sensor_data.temp_int;
						msg.temp_dec=g_sensor_data.temp_dec;
						msg.humi_int=g_sensor_data.humi_int;
						msg.humi_dec=g_sensor_data.humi_dec;
						msg.valid=0;
						msg.update_count=g_sensor_data.update_count;
						msg.error_count=g_sensor_data.error_count;
						msg.alarm_state=ALARM_NONE;
					
						taskEXIT_CRITICAL();
							
					
        }
				if (g_system_event_group != NULL)
				{
						xEventGroupSetBits(g_system_event_group, EVT_SENSOR_RUN);

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
void StartWatchdogTask(void const*argument)
{
	uint32_t last_sensor_total=0;
	uint32_t last_display_heart=0;
	uint32_t last_uart_heartbeat=0;
	
	uint32_t sensor_total=0;
	uint32_t display_now=0;
	uint32_t uart_now=0;
	
	(void)argument;
	osDelay(3000);
	for(;;)
	{
		taskENTER_CRITICAL();
		sensor_total=g_sensor_data.update_count+g_sensor_data.error_count;
		display_now=g_display_heartbeat;
		uart_now=g_uart_heartbeat;
		taskEXIT_CRITICAL();
		if((sensor_total!=last_sensor_total)&&(display_now!=last_display_heart)&&(uart_now!=last_uart_heartbeat))
		{
			HAL_IWDG_Refresh(&hiwdg);
			last_sensor_total=sensor_total;
			last_display_heart=display_now;
			last_uart_heartbeat=uart_now;
		}
		osDelay(3000);
	}
}
void StartKeyTask(void const * argument)
{
    (void)argument;

    for (;;)
    {
        if (Key_IsPressed(KEY_MODE_GPIO_Port, KEY_MODE_Pin))
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

        if (Key_IsPressed(KEY_UP_GPIO_Port,KEY_UP_Pin))
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

        if (Key_IsPressed(KEY_DOWN_GPIO_Port, KEY_DOWN_Pin))
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
        osDelay(50);
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
    EventBits_t wait_bits;
    EventBits_t now_bits;

    const EventBits_t required_bits = EVT_SENSOR_RUN |
                                      EVT_DISPLAY_RUN |
                                      EVT_UART_RUN;

    (void)argument;
    osDelay(3000);

    for (;;)
    {
        wait_bits = xEventGroupWaitBits(
                        g_system_event_group,
                        required_bits,
                        pdTRUE,
                        pdTRUE,
                       pdMS_TO_TICKS(MONITOR_TASK_TIMEOUT_MS)
                    );

        if ((wait_bits & required_bits) == required_bits)
        {
            Serial_SendText("SYS TASK OK\r\n");
        }
        else
        {
            Serial_SendText("SYS TASK TIMEOUT\r\n");
        }

        now_bits = xEventGroupGetBits(g_system_event_group);

        if ((now_bits & EVT_WIFI_OK) != 0)
        {
            Serial_SendText("SYS WIFI OK\r\n");
        }

        if ((now_bits & EVT_WIFI_ERR) != 0)
        {
            Serial_SendText("SYS WIFI ERROR\r\n");
        }

        if ((now_bits & EVT_ALARM_ACTIVE) != 0)
        {
            Serial_SendText("SYS ALARM ACTIVE\r\n");
        }

        if ((now_bits & EVT_CONFIG_SAVED) != 0)
        {
            Serial_SendText("SYS CONFIG SAVED EVT\r\n");
            xEventGroupClearBits(g_system_event_group, EVT_CONFIG_SAVED);
        }

        osDelay(1000);
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

