/**
 * @file test_main.c
 * @brief lw_at_transmit 模块白盒单元测试
 *
 * @details
 * 链接 lw_at_transmit.c 与 lw_at_stream.c，以 LW_AT_CFG_TRANSMIT=1 编译。
 * 覆盖 +++ 前后静默（silent 标志事件驱动）、分片、破坏还原与 sink 转发。
 * 静默判定已改由上层定时器回调置 silent 标志，本模块以布尔参数模拟。
 * 详见 TEST.md。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 1.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei feed 改用 silent 标志；移除 tick 用例        v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
 */
#include <stdio.h>
#include <string.h>

#include "lw_at_stream.h"
#include "lw_at_transmit.h"

#define TEST_RX_SIZE 64U
#define TEST_SINK_CAP 128U

static uint8_t rx_mem[TEST_RX_SIZE];
static lw_at_stream_t stream;
static lw_at_transmit_t tm;

static uint8_t sink_mem[TEST_SINK_CAP];
static uint32_t sink_len;

static uint32_t check_total;
static uint32_t check_fail;

/**
 * @brief 断言并打印
 * @param ok   非 0 通过
 * @param expr 说明
 * @param line 行号
 */
static void test_check(int ok, const char *expr, int line)
{
    check_total++;
    if (ok != 0) {
        (void)printf("[PASS] %s\n", expr);
    } else {
        check_fail++;
        (void)printf("[FAIL] line %d: %s\n", line, expr);
    }
}

#define TEST_CHECK(e) test_check((e) ? 1 : 0, #e, __LINE__)

/**
 * @brief sink：拼接收到的透传数据
 * @param data 数据
 * @param len  长度
 * @param user 未使用
 */
static void test_sink(const uint8_t *data, uint32_t len, void *user)
{
    (void)user;
    if ((sink_len + len) > TEST_SINK_CAP) {
        len = TEST_SINK_CAP - sink_len;
    }
    if (len > 0U) {
        (void)memcpy(&sink_mem[sink_len], data, len);
        sink_len += len;
    }
}

/**
 * @brief 复位 stream / transmit / sink 夹具
 */
static void test_reset_fixture(void)
{
    lw_at_stream_init(&stream, rx_mem, TEST_RX_SIZE);
    lw_at_transmit_init(&tm, 0U);
    sink_len = 0U;
    (void)memset(sink_mem, 0, sizeof(sink_mem));
}

/**
 * @brief 将流中全部数据拷到 out（经 peek/consume）
 * @param out 输出缓冲
 * @param cap 容量
 * @return 拷贝字节数
 */
static uint32_t test_drain_stream(uint8_t *out, uint32_t cap)
{
    const uint8_t *p;
    uint32_t n;
    uint32_t total = 0U;

    while ((n = lw_at_stream_peek(&stream, &p)) > 0U) {
        if ((total + n) > cap) {
            n = cap - total;
        }
        if (n == 0U) {
            break;
        }
        (void)memcpy(&out[total], p, n);
        lw_at_stream_consume(&stream, n);
        total += n;
    }
    return total;
}

/**
 * @brief T01：init
 */
static void test_t01_init(void)
{
    (void)printf("\n==== T01 init ====\n");
    test_reset_fixture();
    (void)printf("<< plus_cnt=%u\n", (unsigned)tm.plus_cnt);
    TEST_CHECK(tm.plus_cnt == 0U);
}

/**
 * @brief T02：reset
 */
static void test_t02_reset(void)
{
    uint8_t exited = 0U;
    const uint8_t plus = (uint8_t)'+';

    (void)printf("\n==== T02 reset 清暂存 ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 1U, &exited);
    TEST_CHECK(tm.plus_cnt == 1U);
    lw_at_transmit_reset(&tm);
    (void)printf("<< after reset plus_cnt=%u\n", (unsigned)tm.plus_cnt);
    TEST_CHECK(tm.plus_cnt == 0U);
}

/**
 * @brief T03：普通数据
 */
static void test_t03_normal_data(void)
{
    uint8_t exited = 0U;
    uint8_t out[16];
    uint32_t n;
    int32_t w;

    (void)printf("\n==== T03 普通数据入流 ====\n");
    test_reset_fixture();
    w = lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"ABC", 3U, 0U,
                            &exited);
    n = test_drain_stream(out, sizeof(out));
    (void)printf("<< written=%ld exited=%u drained=%u out=\"%.*s\"\n", (long)w,
                 (unsigned)exited, (unsigned)n, (int)n, out);
    TEST_CHECK(w == 3);
    TEST_CHECK(exited == 0U);
    TEST_CHECK(n == 3U);
    TEST_CHECK(memcmp(out, "ABC", 3) == 0);
}

/**
 * @brief T04：前静默不足
 */
static void test_t04_gap_too_small_start(void)
{
    uint8_t exited = 0U;
    uint8_t out[8];
    uint32_t n;
    const uint8_t plus = (uint8_t)'+';

    (void)printf("\n==== T04 前静默不足不能起 + ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 0U, &exited);
    n = test_drain_stream(out, sizeof(out));
    (void)printf("<< plus_cnt=%u drained=%u out[0]=0x%02X exited=%u\n",
                 (unsigned)tm.plus_cnt, (unsigned)n,
                 (n > 0U) ? out[0] : 0U, (unsigned)exited);
    TEST_CHECK(tm.plus_cnt == 0U);
    TEST_CHECK(n == 1U);
    TEST_CHECK(out[0] == (uint8_t)'+');
    TEST_CHECK(exited == 0U);
}

/**
 * @brief T05：起候选
 */
static void test_t05_start_plus(void)
{
    uint8_t exited = 0U;
    const uint8_t *p;
    const uint8_t plus = (uint8_t)'+';

    (void)printf("\n==== T05 段首 + 且 silent 满足起候选 ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 1U, &exited);
    (void)printf("<< plus_cnt=%u peek=%u exited=%u\n", (unsigned)tm.plus_cnt,
                 (unsigned)lw_at_stream_peek(&stream, &p), (unsigned)exited);
    TEST_CHECK(tm.plus_cnt == 1U);
    TEST_CHECK(lw_at_stream_peek(&stream, &p) == 0U);
    TEST_CHECK(exited == 0U);
}

/**
 * @brief T06：凑齐 ++
 */
static void test_t06_two_plus(void)
{
    uint8_t exited = 0U;
    const uint8_t plus = (uint8_t)'+';

    (void)printf("\n==== T06 凑齐 ++ 未退出 ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 1U, &exited);
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 0U, &exited);
    (void)printf("<< plus_cnt=%u exited=%u\n", (unsigned)tm.plus_cnt,
                 (unsigned)exited);
    TEST_CHECK(tm.plus_cnt == 2U);
    TEST_CHECK(exited == 0U);
}

/**
 * @brief T07：+++ 后下一段 silent 满足即退出
 */
static void test_t07_feed_exit(void)
{
    uint8_t exited = 0U;
    const uint8_t seq[3] = {'+', '+', '+'};
    uint8_t out[16];
    uint32_t n;
    int32_t w;

    (void)printf("\n==== T07 +++ 后下段 silent=1 退出 ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, seq, 3U, 1U, &exited);
    TEST_CHECK(tm.plus_cnt == 3U);
    TEST_CHECK(exited == 0U);
    w = lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"AT\r\n", 4U, 1U,
                            &exited);
    n = test_drain_stream(out, sizeof(out));
    (void)printf("<< exited=%u written=%ld drained=%u plus_cnt=%u\n",
                 (unsigned)exited, (long)w, (unsigned)n, (unsigned)tm.plus_cnt);
    TEST_CHECK(exited == 1U);
    TEST_CHECK(w == 0);
    TEST_CHECK(tm.plus_cnt == 0U);
    TEST_CHECK(n == 0U);
    /* 上层记下分界后写入确认段 */
    TEST_CHECK(lw_at_stream_feed(&stream, (const uint8_t *)"AT\r\n", 4U) == 4);
    n = test_drain_stream(out, sizeof(out));
    TEST_CHECK(n == 4U);
    TEST_CHECK(memcmp(out, "AT\r\n", 4) == 0);
}

/**
 * @brief T08：后静默不足
 */
static void test_t08_post_guard_break(void)
{
    uint8_t exited = 0U;
    const uint8_t seq[3] = {'+', '+', '+'};
    uint8_t out[16];
    uint32_t n;

    (void)printf("\n==== T08 +++ 后 silent=0 破坏 ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, seq, 3U, 1U, &exited);
    (void)lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"X", 1U, 0U,
                              &exited);
    n = test_drain_stream(out, sizeof(out));
    (void)printf("<< exited=%u drained=%u out=\"%.*s\"\n", (unsigned)exited,
                 (unsigned)n, (int)n, out);
    TEST_CHECK(exited == 0U);
    TEST_CHECK(tm.plus_cnt == 0U);
    TEST_CHECK(n == 4U);
    TEST_CHECK(memcmp(out, "+++X", 4) == 0);
}

/**
 * @brief T09：中途破坏
 */
static void test_t09_mid_break(void)
{
    uint8_t exited = 0U;
    uint8_t out[16];
    uint32_t n;

    (void)printf("\n==== T09 序列中途非 + (++A) ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"++A", 3U, 1U,
                              &exited);
    n = test_drain_stream(out, sizeof(out));
    (void)printf("<< plus_cnt=%u drained=%u out=\"%.*s\"\n",
                 (unsigned)tm.plus_cnt, (unsigned)n, (int)n, out);
    TEST_CHECK(exited == 0U);
    TEST_CHECK(tm.plus_cnt == 0U);
    TEST_CHECK(n == 3U);
    TEST_CHECK(memcmp(out, "++A", 3) == 0);
}

/**
 * @brief T10：非段首 +
 */
static void test_t10_plus_not_first(void)
{
    uint8_t exited = 0U;
    uint8_t out[16];
    uint32_t n;

    (void)printf("\n==== T10 非段首的 + ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"A+", 2U, 1U,
                              &exited);
    n = test_drain_stream(out, sizeof(out));
    (void)printf("<< plus_cnt=%u out=\"%.*s\"\n", (unsigned)tm.plus_cnt,
                 (int)n, out);
    TEST_CHECK(tm.plus_cnt == 0U);
    TEST_CHECK(n == 2U);
    TEST_CHECK(memcmp(out, "A+", 2) == 0);
}

/**
 * @brief T11：同段 +++ 后又有字节
 */
static void test_t11_same_chunk_extra(void)
{
    uint8_t exited = 0U;
    uint8_t out[16];
    uint32_t n;

    (void)printf("\n==== T11 同段凑齐后又有字节 (+++A) ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"+++A", 4U, 1U,
                              &exited);
    n = test_drain_stream(out, sizeof(out));
    (void)printf("<< exited=%u plus_cnt=%u out=\"%.*s\"\n", (unsigned)exited,
                 (unsigned)tm.plus_cnt, (int)n, out);
    TEST_CHECK(exited == 0U);
    TEST_CHECK(tm.plus_cnt == 0U);
    TEST_CHECK(n == 4U);
    TEST_CHECK(memcmp(out, "+++A", 4) == 0);
}

/**
 * @brief T12：guard 关闭（silent 恒满足）
 */
static void test_t12_guard_zero(void)
{
    uint8_t exited = 0U;
    const uint8_t seq[3] = {'+', '+', '+'};

    (void)printf("\n==== T12 silent 恒 1（关闭守卫） ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, seq, 3U, 1U, &exited);
    TEST_CHECK(tm.plus_cnt == 3U);
    (void)lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"A", 1U, 1U,
                              &exited);
    (void)printf("<< exited=%u plus_cnt=%u\n", (unsigned)exited,
                 (unsigned)tm.plus_cnt);
    TEST_CHECK(exited == 1U);
    TEST_CHECK(tm.plus_cnt == 0U);
}

/**
 * @brief T13：分片 +++
 */
static void test_t13_fragmented_plus(void)
{
    uint8_t exited = 0U;
    const uint8_t plus = (uint8_t)'+';

    (void)printf("\n==== T13 跨 feed 分片 +++ ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 1U, &exited);
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 0U, &exited);
    (void)lw_at_transmit_feed(&tm, &stream, &plus, 1U, 0U, &exited);
    TEST_CHECK(tm.plus_cnt == 3U);
    (void)lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"AT\r\n", 4U, 1U,
                              &exited);
    (void)printf("<< plus_cnt_after_frag=3 then exit=%u\n", (unsigned)exited);
    TEST_CHECK(exited == 1U);
    TEST_CHECK(tm.plus_cnt == 0U);
}

/**
 * @brief T14：process → sink
 */
static void test_t14_process_sink(void)
{
    uint8_t exited = 0U;
    const uint8_t *p;

    (void)printf("\n==== T14 process 转发 sink ====\n");
    test_reset_fixture();
    (void)lw_at_transmit_feed(&tm, &stream, (const uint8_t *)"HELLO", 5U, 0U,
                              &exited);
    /* 人为置溢出再 process，应被清除且仍转发已缓存数据 */
    stream.overflow = 1U;
    lw_at_transmit_process(&stream, test_sink, NULL);
    (void)printf("<< sink_len=%u sink=\"%.*s\" peek=%u overflow_take=%u\n",
                 (unsigned)sink_len, (int)sink_len, sink_mem,
                 (unsigned)lw_at_stream_peek(&stream, &p),
                 (unsigned)lw_at_stream_overflow_take(&stream));
    TEST_CHECK(sink_len == 5U);
    TEST_CHECK(memcmp(sink_mem, "HELLO", 5) == 0);
    TEST_CHECK(lw_at_stream_peek(&stream, &p) == 0U);
    TEST_CHECK(lw_at_stream_overflow_take(&stream) == 0U);
}

int main(void)
{
    (void)printf("lw_at_transmit unit test\n");
    (void)printf("see TEST.md for items / reasons / expected results\n");

    test_t01_init();
    test_t02_reset();
    test_t03_normal_data();
    test_t04_gap_too_small_start();
    test_t05_start_plus();
    test_t06_two_plus();
    test_t07_feed_exit();
    test_t08_post_guard_break();
    test_t09_mid_break();
    test_t10_plus_not_first();
    test_t11_same_chunk_extra();
    test_t12_guard_zero();
    test_t13_fragmented_plus();
    test_t14_process_sink();

    (void)printf("\n==== 汇总 ====\n");
    (void)printf("checks: %u, failed: %u\n", (unsigned)check_total,
                 (unsigned)check_fail);
    return (check_fail == 0U) ? 0 : 1;
}
