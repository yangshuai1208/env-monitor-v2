#include "sensor_data.h"

#include <stddef.h>

void SensorData_Init(SensorData *data)
{
    if (data == NULL)
    {
        return;
    }
    SensorData sensor_data={0};
    *data=sensor_data;
    /* TODO：把所有字段初始化为0 */
}

AlarmState SensorData_CheckAlarm(
    uint8_t temp_int,
    uint8_t humi_int,
    int16_t temp_alarm_high,
    int16_t humi_alarm_low)
{
    if ((temp_int >= temp_alarm_high) &&
        (humi_int <= humi_alarm_low))
    {
        return ALARM_BOTH;
    }
    else if (temp_int >= temp_alarm_high)
    {
        return ALARM_TEMP_HIGH;
    }
    else if (humi_int <= humi_alarm_low)
    {
        return ALARM_HUMI_LOW;
    }
    else
    {
        return ALARM_NONE;
    }
}

const char *SensorData_AlarmToString(AlarmState state)
{
    switch (state)
    {
        case ALARM_NONE:
            return "NORMAL";
        case ALARM_TEMP_HIGH:
            return "TEMP_HIGH";
        case ALARM_HUMI_LOW:
            return "HUMI_LOW";
        case ALARM_BOTH:
            return "TEMP_HIGH_HUMI_LOW";
        default:
            return "UNKNOWN";
    }
 
    /* TODO：把枚举转换成字符串 */
}
