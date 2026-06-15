
#include "bsp_dht11.h"
#include "bsp_dwt.h"


void DHT11_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET);
}

void DHT11_SetGPIOMode(uint32_t mode, uint32_t pull)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DHT11_DATA_Pin;
    GPIO_InitStruct.Mode = mode;
    GPIO_InitStruct.Pull = pull;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_DATA_GPIO_Port, &GPIO_InitStruct);
}

#define DHT11_SIGNAL_TIMEOUT_US 120U

static HAL_StatusTypeDef DHT11_WaitWhile(GPIO_PinState state, uint32_t timeout_us)
{
    while (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == state)
    {
        if (timeout_us == 0U)
        {
            return HAL_TIMEOUT;
        }
        timeout_us--;
        DWT_DelayUs(1);
    }

    return HAL_OK;
}

static HAL_StatusTypeDef DHT11_ReadByteChecked(uint8_t *value)
{
    uint8_t byte_value = 0;

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    for (uint8_t i = 0; i < 8; i++)
    {
        if (DHT11_WaitWhile(GPIO_PIN_RESET, DHT11_SIGNAL_TIMEOUT_US) != HAL_OK)
        {
            return HAL_TIMEOUT;
        }

        DWT_DelayUs(40);

        if (HAL_GPIO_ReadPin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin) == GPIO_PIN_SET)
        {
            byte_value |= (uint8_t)(1U << (7U - i));

            if (DHT11_WaitWhile(GPIO_PIN_SET, DHT11_SIGNAL_TIMEOUT_US) != HAL_OK)
            {
                return HAL_TIMEOUT;
            }
        }
    }

    *value = byte_value;
    return HAL_OK;
}

uint8_t DHT11_ReadByte(void)
{
    uint8_t value = 0;

    (void)DHT11_ReadByteChecked(&value);
    return value;
}


/**
 * @brief  Read DHT11 humidity and temperature data.
 * @param  data Output buffer.
 * @retval HAL_OK on checksum success, HAL_ERROR on timeout or checksum error.
 */
HAL_StatusTypeDef DHT11_ReadData(DHT11_DATA_TYPEDEF *data)
{
    uint8_t sum = 0;

    if (data == NULL)
    {
        return HAL_ERROR;
    }

    DHT11_SetGPIOMode(GPIO_MODE_OUTPUT_PP, GPIO_NOPULL);
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_RESET);
    DWT_DelayMs(20);
    HAL_GPIO_WritePin(DHT11_DATA_GPIO_Port, DHT11_DATA_Pin, GPIO_PIN_SET);
    DWT_DelayUs(30);
    DHT11_SetGPIOMode(GPIO_MODE_INPUT, GPIO_PULLUP);

    if (DHT11_WaitWhile(GPIO_PIN_SET, DHT11_SIGNAL_TIMEOUT_US) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (DHT11_WaitWhile(GPIO_PIN_RESET, DHT11_SIGNAL_TIMEOUT_US) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (DHT11_WaitWhile(GPIO_PIN_SET, DHT11_SIGNAL_TIMEOUT_US) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if (DHT11_ReadByteChecked(&data->humi_int) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (DHT11_ReadByteChecked(&data->humi_deci) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (DHT11_ReadByteChecked(&data->temp_int) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (DHT11_ReadByteChecked(&data->temp_deci) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if (DHT11_ReadByteChecked(&data->check_sum) != HAL_OK)
    {
        return HAL_ERROR;
    }

    sum = data->humi_int + data->humi_deci + data->temp_int + data->temp_deci;
    return (sum == data->check_sum) ? HAL_OK : HAL_ERROR;
}


/*********************************************END OF FILE**********************/
