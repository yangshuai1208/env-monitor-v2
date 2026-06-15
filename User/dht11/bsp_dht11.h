#ifndef __BSP_DHT11_H
#define __BSP_DHT11_H

#include "main.h"

#define DHT11_DATA_Pin        GPIO_PIN_12
#define DHT11_DATA_GPIO_Port  GPIOB

typedef struct
{
    uint8_t humi_int;
    uint8_t humi_deci;
    uint8_t temp_int;
    uint8_t temp_deci;
    uint8_t check_sum;
} DHT11_DATA_TYPEDEF;

void DHT11_GPIO_Config(void);
void DHT11_SetGPIOMode(uint32_t mode, uint32_t pull);
uint8_t DHT11_ReadByte(void);
HAL_StatusTypeDef DHT11_ReadData(DHT11_DATA_TYPEDEF *dht11_data);

#endif /* __BSP_DHT11_H */
