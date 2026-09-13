#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static bool find_single_number(
    const int32_t *values,
    size_t count,
    int32_t *output
)
{
    int32_t result = 0;

    if ((values == NULL) ||
        (output == NULL) ||
        (count == 0U) ||
        ((count & 1U) == 0U))
    {
        return false;
    }

    for (size_t i = 0U; i < count; ++i)
    {
        /* TODO 1：使用异或累计 */
        if(values[i]!=0)
        {
            result ^=values[i];
        }
    }

    *output = result;
    return true;
}

static uint32_t count_set_bits(uint32_t value)
{
    uint32_t count = 0U;

    while (value != 0U)
    {
        /* TODO 2：清除最低位的一个1 */
        value &=(value-1);
        count++;
    }

    return count;
}

int main(void)
{
    const int32_t test1[] =
    {
        4, 1, 2, 1, 2
    };

    const int32_t test2[] =
    {
        -3, 9, -3
    };

    int32_t output = 0;

    assert(
        find_single_number(
            test1,
            sizeof(test1) / sizeof(test1[0]),
            &output
        )
    );
    assert(output == 4);

    assert(
        find_single_number(
            test2,
            sizeof(test2) / sizeof(test2[0]),
            &output
        )
    );
    assert(output == 9);

    assert(!find_single_number(
        NULL,
        3U,
        &output
    ));

    assert(!find_single_number(
        test1,
        0U,
        &output
    ));

    assert(!find_single_number(
        test1,
        4U,
        &output
    ));

    assert(!find_single_number(
        test1,
        5U,
        NULL
    ));

    assert(count_set_bits(0U) == 0U);
    assert(count_set_bits(0xB0U) == 3U);
    assert(count_set_bits(0xFFFFFFFFU) == 32U);

    printf("bit algorithm tests passed\n");
    return 0;
}