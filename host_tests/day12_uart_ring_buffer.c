#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

#define UART_RING_CAPACITY 8U

typedef struct
{
    uint8_t data[UART_RING_CAPACITY];

    /*
     * 下一个写入位置。
     */
    size_t head;
    /*
     * 下一个读取位置。
     */
    size_t tail;

    /*
     * 当前已有数据数量。
     */
    size_t count;
} UartRingBuffer;

static bool uart_ring_init(
    UartRingBuffer *ring)
{
    /*
     * TODO：
     * 1. ring == NULL时返回false。
     * 2. 使用局部临时结构体初始化。
     * 3. head、tail、count全部为0。
     * 4. 成功返回true。
     */
    if(ring==NULL)
    {
        return false;
    }
   UartRingBuffer temp = {0};
   *ring=temp;
    return true;
}

static bool uart_ring_push(
    UartRingBuffer *ring,
    uint8_t byte)
{
    /*
     * TODO：
     * 1. 检查ring。
     * 2. count已经达到容量时返回false。
     * 3. 失败时不能覆盖旧数据。
     * 4. 将byte写入data[head]。
     * 5. head向后移动并处理回绕。
     * 6. count增加。
     */
    if(ring==NULL)
    {
        return false;
    }
    if(ring->count>=UART_RING_CAPACITY)
    {
        return false;
    }
    ring->data[ring->head]=byte;
    ++ring->head;
    if(ring->head==UART_RING_CAPACITY)
    {
        ring->head=0U;
    }
    ring->count++;
    return true;
}

static bool uart_ring_pop(
    UartRingBuffer *ring,
    uint8_t *byte_out)
{
    /*
     * TODO：
     * 1. 检查ring和byte_out。
     * 2. count为0时返回false。
     * 3. 失败时不能修改byte_out。
     * 4. 读取data[tail]。
     * 5. tail向后移动并处理回绕。
     * 6. count减少。
     * 7. 成功后才修改byte_out。
     */
    if(ring==NULL||byte_out==NULL)
    {
        return false;
    }
    if(ring->count==0U)
    {
        return false;
    }
    *byte_out=ring->data[ring->tail];
    ++ring->tail;
    if(ring->tail==UART_RING_CAPACITY)
    {
        ring->tail=0U;
    }
    ring->count--;
    return true;
}
static void test_initialization(void)
{
    UartRingBuffer ring =
    {
        {0xAAU},
        5U,
        6U,
        7U
    };

    assert(!uart_ring_init(NULL));

    assert(uart_ring_init(&ring));

    assert(ring.head == 0U);
    assert(ring.tail == 0U);
    assert(ring.count == 0U);
}

static void test_fifo_order(void)
{
    UartRingBuffer ring;
    uint8_t output = 0U;

    assert(uart_ring_init(&ring));

    assert(uart_ring_push(&ring, 0x11U));
    assert(uart_ring_push(&ring, 0x22U));
    assert(uart_ring_push(&ring, 0x33U));

    assert(ring.count == 3U);

    assert(uart_ring_pop(&ring, &output));
    assert(output == 0x11U);

    assert(uart_ring_pop(&ring, &output));
    assert(output == 0x22U);

    assert(uart_ring_pop(&ring, &output));
    assert(output == 0x33U);

    assert(ring.count == 0U);
}

static void test_full_buffer(void)
{
    UartRingBuffer ring;

    assert(uart_ring_init(&ring));

    for (uint8_t value = 0U;
         value < UART_RING_CAPACITY;
         ++value)
    {
        assert(uart_ring_push(
            &ring,
            value
        ));
    }

    assert(ring.count ==
           UART_RING_CAPACITY);

    /*
     * 缓冲区已满，不能覆盖旧数据。
     */
    assert(!uart_ring_push(
        &ring,
        0xFFU
    ));

    assert(ring.count ==
           UART_RING_CAPACITY);
}

static void test_empty_buffer(void)
{
    UartRingBuffer ring;
    uint8_t output = 0xA5U;

    assert(uart_ring_init(&ring));

    assert(!uart_ring_pop(
        &ring,
        &output
    ));

    /*
     * 读取失败不能修改输出。
     */
    assert(output == 0xA5U);
    assert(ring.count == 0U);
}

static void test_wraparound(void)
{
    UartRingBuffer ring;
    uint8_t output = 0U;

    assert(uart_ring_init(&ring));

    /*
     * 先写入0～5。
     */
    for (uint8_t value = 0U;
         value < 6U;
         ++value)
    {
        assert(uart_ring_push(
            &ring,
            value
        ));
    }

    /*
     * 读出0～3。
     */
    for (uint8_t expected = 0U;
         expected < 4U;
         ++expected)
    {
        assert(uart_ring_pop(
            &ring,
            &output
        ));

        assert(output == expected);
    }

    /*
     * 再写入6～11，触发数组回绕。
     */
    for (uint8_t value = 6U;
         value < 12U;
         ++value)
    {
        assert(uart_ring_push(
            &ring,
            value
        ));
    }

    assert(ring.count ==
           UART_RING_CAPACITY);

    /*
     * 应按FIFO顺序读出4～11。
     */
    for (uint8_t expected = 4U;
         expected < 12U;
         ++expected)
    {
        assert(uart_ring_pop(
            &ring,
            &output
        ));

        assert(output == expected);
    }

    assert(ring.count == 0U);
}

static void test_invalid_arguments(void)
{
    UartRingBuffer ring;
    uint8_t output = 0U;

    assert(uart_ring_init(&ring));

    assert(!uart_ring_push(
        NULL,
        0x11U
    ));

    assert(!uart_ring_pop(
        NULL,
        &output
    ));

    assert(!uart_ring_pop(
        &ring,
        NULL
    ));
}

int main(void)
{
    test_initialization();
    test_fifo_order();
    test_full_buffer();
    test_empty_buffer();
    test_wraparound();
    test_invalid_arguments();

    printf("UART ring buffer tests passed\n");

    return 0;
}