#ifndef __BSP_FLASH_PARAM_H
#define __BSP_FLASH_PARAM_H
#include "main.h"

#define APP_CONFIG_FLASH_ADDR    0x0800FC00U
#define APP_CONFIG_FLASH_PAGE_SIZE  1024U

#define APP_CONFIG_MAGIC          0x20260422U

typedef struct
{
	uint32_t magic;
	uint32_t sample_period_ms;
	uint32_t temp_alarm_high;
	uint32_t humi_alarm_low;
	uint32_t device_id;
	uint32_t debug_enable;
	uint32_t checksum;
	

}AppConfig;



void AppConfig_SetDefault(AppConfig*config);
uint16_t AppConfig_CalcChecksum(const AppConfig*config);
uint8_t AppConfig_Load(AppConfig*config);
uint8_t AppConfig_Save(const AppConfig*config);



#endif
