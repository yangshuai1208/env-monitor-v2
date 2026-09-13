#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct
{
    uint32_t last_alive_tick;
    uint32_t deadline_ticks;
    uint8_t seen;
} TaskHealth;

uint32_t collect_expired_mask(
    const TaskHealth *tasks,
    size_t count,
    uint32_t now_tick
)
{
    uint32_t mask = 0U;

    if ((tasks == NULL) || (count > 32U))
    {
        return UINT32_MAX;
    }

    for (size_t i = 0U; i < count; ++i)
    {
        uint32_t elapsed =
            now_tick - tasks[i].last_alive_tick;

        if ((tasks[i].seen == 0U) ||elapsed>tasks[i].deadline_ticks)
            /* TODO 1：判断elapsed是否超过deadline */
        {
            /* TODO 2：将第i位置1 */
            mask |=(1U<<i);
        }
    }

    return mask;
}

int main(void)
{
    const TaskHealth normal_test[] =
    {
        {100U, 50U, 1U},
        {100U, 39U, 1U},
        {0U,   100U, 0U}
    };

    /*
     * now=140：
     * task0经过40 Tick，未超时。
     * task1经过40 Tick，超过39，超时。
     * task2从未报告，超时。
     * 结果应为bit1和bit2置位：0x06。
     */
    assert(
        collect_expired_mask(
            normal_test,
            3U,
            140U
        ) == 0x06U
    );

    const TaskHealth boundary_test[] =
    {
        {100U, 40U, 1U}
    };

    /* 当前源码使用>，刚好等于deadline不算超时。 */
    assert(
        collect_expired_mask(
            boundary_test,
            1U,
            140U
        ) == 0U
    );

    const TaskHealth wrap_test[] =
    {
        {0xFFFFFFF0U, 64U, 1U},
        {0xFFFFFFF0U, 32U, 1U}
    };

    /*
     * now-last = 0x30 = 48 Tick。
     * task0未超时，task1超时。
     */
    assert(
        collect_expired_mask(
            wrap_test,
            2U,
            0x00000020U
        ) == 0x02U
    );

    printf("all tests passed\n");
    return 0;
}