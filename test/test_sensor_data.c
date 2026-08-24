#include "sensor_data.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_init(void)
{
    SensorData data =
    {
        1, 1, 1, 1,
        1, 1,
        1, 1,
        ALARM_BOTH
    };

    SensorData_Init(&data);

    assert(data.temp_int == 0U);
    assert(data.temp_dec == 0U);
    assert(data.humi_int == 0U);
    assert(data.humi_dec == 0U);
    assert(data.ready == 0U);
    assert(data.valid == 0U);
    assert(data.update_count == 0U);
    assert(data.error_count == 0U);
    assert(data.alarm_state == ALARM_NONE);

    /* 不应崩溃 */
    SensorData_Init(NULL);
}

static void test_alarm(void)
{
    assert(SensorData_CheckAlarm(
        25U, 60U, 30, 40) == ALARM_NONE);

    /* 测试等于温度阈值 */
    assert(SensorData_CheckAlarm(
        30U, 60U, 30, 40) == ALARM_TEMP_HIGH);

    /* 测试等于湿度阈值 */
    assert(SensorData_CheckAlarm(
        25U, 40U, 30, 40) == ALARM_HUMI_LOW);

    assert(SensorData_CheckAlarm(
        35U, 30U, 30, 40) == ALARM_BOTH);
}

static void test_alarm_string(void)
{
    assert(strcmp(
        SensorData_AlarmToString(ALARM_NONE),
        "NORMAL") == 0);

    assert(strcmp(
        SensorData_AlarmToString(ALARM_TEMP_HIGH),
        "TEMP_HIGH") == 0);

    assert(strcmp(
        SensorData_AlarmToString(ALARM_HUMI_LOW),
        "HUMI_LOW") == 0);

    assert(strcmp(
        SensorData_AlarmToString(ALARM_BOTH),
        "TEMP_HIGH_HUMI_LOW") == 0);

    assert(strcmp(
        SensorData_AlarmToString((AlarmState)100),
        "UNKNOWN") == 0);
}

int main(void)
{
    test_init();
    test_alarm();
    test_alarm_string();

    printf("sensor data tests passed\n");
    return 0;
}