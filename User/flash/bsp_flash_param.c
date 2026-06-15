#include "bsp_flash_param.h"
#include <string.h>
#include <stddef.h>


void AppConfig_SetDefault(AppConfig*config)
{
	if(config==NULL)
	{
		return;
	
	}

	config->magic=APP_CONFIG_MAGIC;
	config->sample_period_ms=2000;
	config->temp_alarm_high=30;
	config->humi_alarm_low=40;
	config->device_id=1;
	config->debug_enable=1;
	config->checksum=AppConfig_CalcChecksum(config);
}
uint16_t AppConfig_CalcChecksum(const AppConfig*config)
{
	const uint8_t*p=NULL;
	uint32_t sum=0;
	uint32_t i=0;
	if(config==NULL)
	{
		return 0;


	}
	p=(const uint8_t*)config;

	for(i=0;i<offsetof(AppConfig,checksum);i++)
	{
	
		sum+=p[i];
	
	}
	return (uint16_t)(sum&0xFFFF);
	
}
uint8_t AppConfig_Load(AppConfig*config)
{
	AppConfig flash_config;
	if(config==NULL)
	{
		return 1;
	}
	memcpy(&flash_config,(const void*)APP_CONFIG_FLASH_ADDR,sizeof(AppConfig));
	if(flash_config.magic!=APP_CONFIG_MAGIC)
	{
		return 1;
	
	}
	if(flash_config.checksum!=AppConfig_CalcChecksum(&flash_config))
	{
		return 2;
	
	}
		memcpy(config,&flash_config,sizeof(AppConfig));
		return 0;
}
uint8_t AppConfig_Save(const AppConfig *config)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    uint32_t addr = APP_CONFIG_FLASH_ADDR;
    uint32_t i = 0;
    uint16_t half_word = 0;
    AppConfig temp_config;

    if (config == NULL)
    {
        return 1;
    }

    memcpy(&temp_config, config, sizeof(AppConfig));
    temp_config.magic = APP_CONFIG_MAGIC;
    temp_config.checksum = AppConfig_CalcChecksum(&temp_config);

    HAL_FLASH_Unlock();

    erase_init.TypeErase = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = APP_CONFIG_FLASH_ADDR;
    erase_init.NbPages = 1;

    status = HAL_FLASHEx_Erase(&erase_init, &page_error);
    if (status != HAL_OK)
    {
        HAL_FLASH_Lock();
        return 2;
    }

    for (i = 0; i < sizeof(AppConfig); i += 2)
    {
        half_word = *(uint16_t *)((uint8_t *)&temp_config + i);

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                                   addr + i,
                                   half_word);

        if (status != HAL_OK)
        {
            HAL_FLASH_Lock();
            return 3;
        }
    }

    HAL_FLASH_Lock();

    return 0;
}
