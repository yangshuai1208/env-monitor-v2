#ifndef SENSOR_DATA_H
#define SENSOR_DATA_H

#include <stdint.h>

typedef enum
{
    ALARM_NONE = 0,
    ALARM_TEMP_HIGH,
    ALARM_HUMI_LOW,
    ALARM_BOTH
} AlarmState;

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

void SensorData_Init(SensorData *data);

AlarmState SensorData_CheckAlarm(
    uint8_t temp_int,
    uint8_t humi_int,
    int16_t temp_alarm_high,
    int16_t humi_alarm_low);

const char *SensorData_AlarmToString(AlarmState state);

#endif