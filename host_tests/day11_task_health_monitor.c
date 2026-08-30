#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#define TASK_SENSOR_BIT   (1UL << 0)
#define TASK_DISPLAY_BIT  (1UL << 1)
#define TASK_ALARM_BIT    (1UL << 2)
#define TASK_WIFI_BIT     (1UL << 3)
#define REQUIRED_TASKS \
    (TASK_SENSOR_BIT  | \
     TASK_DISPLAY_BIT | \
     TASK_ALARM_BIT   | \
     TASK_WIFI_BIT)

typedef struct
{
    uint32_t required_mask;
    uint32_t reported_mask;
} TaskHealthMonitor;

static bool task_health_init(
    TaskHealthMonitor *monitor,
    uint32_t required_mask)
{
    /*
     * TODO：
     * 1. monitor == NULL，返回false。
     * 2. required_mask == 0，返回false。
     * 3. 参数错误不能修改monitor。
     * 4. required_mask保存传入值。
     * 5. reported_mask初始化为0。
     * 6. 成功返回true。
     */
    if(monitor==NULL)
    {
        return false;
    }
    if(required_mask==0)
    {
        return false;
    }
    monitor->required_mask=required_mask;
    monitor->reported_mask=0;
    return true;
}

static bool task_health_report(
    TaskHealthMonitor *monitor,
    uint32_t task_bit)
{
    /*
     * TODO：
     * 1. 检查monitor。
     * 2. task_bit不能为0。
     * 3. task_bit必须只有一个二进制位为1。
     * 4. task_bit必须属于required_mask。
     * 5. 使用按位或记录心跳。
     * 6. 重复上报同一任务应该允许。
     */
    if(monitor==NULL)
    {
        return false;
    }
    if(task_bit==0)
    {
        return false;
    }
    if(task_bit&(task_bit-1))
    {
        return false;
    }
    if((task_bit&monitor->required_mask)==0)
    {
        return false;
    }
    monitor->reported_mask |= task_bit;
    return true;

}
static bool task_health_evaluate_and_reset(
    TaskHealthMonitor *monitor,
    uint32_t *missing_mask,
    bool *should_feed)
{
    if (monitor == NULL ||
        missing_mask == NULL ||
        should_feed == NULL ||
        monitor->required_mask == 0U)
    {
        return false;
    }

    uint32_t missing =
        monitor->required_mask &
        ~monitor->reported_mask;

    bool feed = (missing == 0U);

    /*
     * 一次检查结束，开始下一个监控周期。
     */
    monitor->reported_mask = 0U;

    /*
     * 所有计算成功后再修改输出参数。
     */
    *missing_mask = missing;
    *should_feed = feed;

    return true;
}
static void test_initialization(void)
{
    TaskHealthMonitor monitor =
    {
        0x11111111UL,
        0x22222222UL
    };

    assert(!task_health_init(
        NULL,
        REQUIRED_TASKS
    ));

    assert(!task_health_init(
        &monitor,
        0U
    ));

    /*
     * 初始化失败不能修改原结构体。
     */
    assert(monitor.required_mask ==
           0x11111111UL);
    assert(monitor.reported_mask ==
           0x22222222UL);

    assert(task_health_init(
        &monitor,
        REQUIRED_TASKS
    ));

    assert(monitor.required_mask ==
           REQUIRED_TASKS);
    assert(monitor.reported_mask == 0U);
}

static void test_all_tasks_healthy(void)
{
    TaskHealthMonitor monitor;
    uint32_t missing = UINT32_MAX;
    bool should_feed = false;

    assert(task_health_init(
        &monitor,
        REQUIRED_TASKS
    ));

    assert(task_health_report(
        &monitor,
        TASK_SENSOR_BIT
    ));

    assert(task_health_report(
        &monitor,
        TASK_DISPLAY_BIT
    ));

    assert(task_health_report(
        &monitor,
        TASK_ALARM_BIT
    ));

    assert(task_health_report(
        &monitor,
        TASK_WIFI_BIT
    ));

    /*
     * 重复上报应该允许。
     */
    assert(task_health_report(
        &monitor,
        TASK_SENSOR_BIT
    ));

    assert(task_health_evaluate_and_reset(
        &monitor,
        &missing,
        &should_feed
    ));

    assert(missing == 0U);
    assert(should_feed);
    assert(monitor.reported_mask == 0U);
}

static void test_missing_wifi_task(void)
{
    TaskHealthMonitor monitor;
    uint32_t missing = 0U;
    bool should_feed = true;

    assert(task_health_init(
        &monitor,
        REQUIRED_TASKS
    ));

    assert(task_health_report(
        &monitor,
        TASK_SENSOR_BIT
    ));

    assert(task_health_report(
        &monitor,
        TASK_DISPLAY_BIT
    ));

    assert(task_health_report(
        &monitor,
        TASK_ALARM_BIT
    ));

    assert(task_health_evaluate_and_reset(
        &monitor,
        &missing,
        &should_feed
    ));

    assert(missing == TASK_WIFI_BIT);
    assert(!should_feed);
    assert(monitor.reported_mask == 0U);
}

static void test_new_monitoring_window(void)
{
    TaskHealthMonitor monitor;
    uint32_t missing = 0U;
    bool should_feed = true;

    assert(task_health_init(
        &monitor,
        REQUIRED_TASKS
    ));

    /*
     * 新周期中一个任务都没有上报。
     */
    assert(task_health_evaluate_and_reset(
        &monitor,
        &missing,
        &should_feed
    ));

    assert(missing == REQUIRED_TASKS);
    assert(!should_feed);
}

static void test_invalid_task_bits(void)
{
    TaskHealthMonitor monitor;

    assert(task_health_init(
        &monitor,
        REQUIRED_TASKS
    ));

    assert(!task_health_report(
        NULL,
        TASK_SENSOR_BIT
    ));

    assert(!task_health_report(
        &monitor,
        0U
    ));

    /*
     * 一次传入多个任务位，必须拒绝。
     */
    assert(!task_health_report(
        &monitor,
        TASK_SENSOR_BIT |
        TASK_WIFI_BIT
    ));

    /*
     * 未注册的任务位，必须拒绝。
     */
    assert(!task_health_report(
        &monitor,
        (1UL << 8)
    ));

    assert(monitor.reported_mask == 0U);
}

static void test_invalid_evaluation_arguments(void)
{
    TaskHealthMonitor monitor;
    uint32_t missing = 0xA5A5A5A5UL;
    bool should_feed = true;

    assert(task_health_init(
        &monitor,
        REQUIRED_TASKS
    ));

    assert(task_health_report(
        &monitor,
        TASK_SENSOR_BIT
    ));

    assert(!task_health_evaluate_and_reset(
        NULL,
        &missing,
        &should_feed
    ));

    assert(!task_health_evaluate_and_reset(
        &monitor,
        NULL,
        &should_feed
    ));

    assert(!task_health_evaluate_and_reset(
        &monitor,
        &missing,
        NULL
    ));

    /*
     * 参数错误不能清空已经收到的心跳，
     * 也不能修改输出参数。
     */
    assert(monitor.reported_mask ==
           TASK_SENSOR_BIT);
    assert(missing == 0xA5A5A5A5UL);
    assert(should_feed);
}

int main(void)
{
    test_initialization();
    test_all_tasks_healthy();
    test_missing_wifi_task();
    test_new_monitoring_window();
    test_invalid_task_bits();
    test_invalid_evaluation_arguments();

    printf("Task health monitor tests passed\n");

    return 0;
}