/**
 * @file test_main.c
 * @brief lw_at_stream 模块白盒单元测试
 *
 * @details
 * 仅链接 lw_at_stream.c，覆盖环形缓存、取行、溢出、peek/consume、
 * 空闲定时等边界。每步打印操作与观测结果，供人工核对。
 * 详见同目录 TEST.md。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-30
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes    version
 * 2026-07-30 linzhiwei 首次发布 v1.0.0
 */
#include <stdio.h>
#include <string.h>

#include "lw_at_stream.h"

/* 常规环形缓存字节数（容量 = size - 1） */
#define TEST_RX_SIZE 32U

/* 取行输出区大小 */
#define TEST_OUT_SIZE 64U

/* 行超长用例：故意更小的输出区 */
#define TEST_OUT_SMALL 4U

/* 转义打印缓冲 */
#define TEST_ESC_SIZE 256U

/* 环形缓存与输出区 */
static uint8_t rx_mem[TEST_RX_SIZE];
static char out_mem[TEST_OUT_SIZE];
static lw_at_stream_t stream;

static uint32_t check_total;
static uint32_t check_fail;

/**
 * @brief 将字节序列转义为可见字符串
 * @param data 数据
 * @param len  长度
 * @return 静态转义缓冲
 */
static const char *test_esc_bin(const uint8_t *data, uint32_t len)
{
    static char esc[TEST_ESC_SIZE];
    uint32_t i = 0U;
    uint32_t k;

    for (k = 0U; (k < len) && (i < (TEST_ESC_SIZE - 5U)); k++) {
        uint8_t b = data[k];

        if (b == (uint8_t)'\r') {
            esc[i++] = '\\';
            esc[i++] = 'r';
        } else if (b == (uint8_t)'\n') {
            esc[i++] = '\\';
            esc[i++] = 'n';
        } else if ((b < 0x20U) || (b >= 0x7FU)) {
            (void)snprintf(&esc[i], TEST_ESC_SIZE - i, "\\x%02X", b);
            i += 4U;
        } else {
            esc[i++] = (char)b;
        }
    }
    esc[i] = '\0';
    return esc;
}

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

#define TEST_CHECK(cond) test_check((cond) ? 1 : 0, #cond, __LINE__)

/**
 * @brief 重建干净的 stream 实例
 */
static void test_stream_reset_fixture(void)
{
    memset(rx_mem, 0, sizeof(rx_mem));
    memset(out_mem, 0, sizeof(out_mem));
    lw_at_stream_init(&stream, rx_mem, TEST_RX_SIZE);
}

/**
 * @brief 喂入字符串（按字节长度，含内部 \\0 时勿用本函数）
 * @param s 字符串
 * @return feed 返回值
 */
static int32_t test_feed_str(const char *s)
{
    return lw_at_stream_feed(&stream, (const uint8_t *)s, (uint32_t)strlen(s));
}

/**
 * @brief S01：初始为空
 */
static void test_s01_init_empty(void)
{
    const uint8_t *p = NULL;

    (void)printf("\n==== S01 init 后缓存为空 ====\n");
    test_stream_reset_fixture();
    (void)printf("<< peek=%u get_line=%ld overflow=%u pending=%u\n",
                 (unsigned)lw_at_stream_peek(&stream, &p),
                 (long)lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE),
                 (unsigned)lw_at_stream_overflow_take(&stream),
                 (unsigned)lw_at_stream_pending_take(&stream));
    TEST_CHECK(lw_at_stream_peek(&stream, &p) == 0U);
    TEST_CHECK(lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE) ==
               LW_AT_STREAM_NO_LINE);
}

/**
 * @brief S02：整行一次写入
 */
static void test_s02_full_line(void)
{
    int32_t n;

    (void)printf("\n==== S02 整行一次写入并可取出 ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"%s\"\n", test_esc_bin((const uint8_t *)"AT\r\n", 4U));
    TEST_CHECK(test_feed_str("AT\r\n") == 4);
    n = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< get_line=%ld out=\"%s\"\n", (long)n, out_mem);
    TEST_CHECK(n == 2);
    TEST_CHECK(strcmp(out_mem, "AT") == 0);
    TEST_CHECK(lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE) ==
               LW_AT_STREAM_NO_LINE);
}

/**
 * @brief S03：按字节分片
 */
static void test_s03_byte_fragment(void)
{
    static const uint8_t a = (uint8_t)'A';
    static const uint8_t t = (uint8_t)'T';
    static const uint8_t cr = (uint8_t)'\r';
    static const uint8_t lf = (uint8_t)'\n';
    int32_t n;

    (void)printf("\n==== S03 字节级分片组帧 ====\n");
    test_stream_reset_fixture();
    (void)printf(">> A | T | \\r | \\n\n");
    TEST_CHECK(lw_at_stream_feed(&stream, &a, 1U) == 1);
    TEST_CHECK(lw_at_stream_feed(&stream, &t, 1U) == 1);
    TEST_CHECK(lw_at_stream_feed(&stream, &cr, 1U) == 1);
    TEST_CHECK(lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE) ==
               LW_AT_STREAM_NO_LINE);
    TEST_CHECK(lw_at_stream_feed(&stream, &lf, 1U) == 1);
    n = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< get_line=%ld out=\"%s\"\n", (long)n, out_mem);
    TEST_CHECK(n == 2);
    TEST_CHECK(strcmp(out_mem, "AT") == 0);
}

/**
 * @brief S04：任意分片
 */
static void test_s04_arb_fragment(void)
{
    int32_t n;

    (void)printf("\n==== S04 任意分片组帧 ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"AT+\" | \"ECHO\\r\" | \"\\n\"\n");
    TEST_CHECK(test_feed_str("AT+") == 3);
    TEST_CHECK(test_feed_str("ECHO\r") == 5);
    TEST_CHECK(test_feed_str("\n") == 1);
    n = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< get_line=%ld out=\"%s\"\n", (long)n, out_mem);
    TEST_CHECK(n == 7);
    TEST_CHECK(strcmp(out_mem, "AT+ECHO") == 0);
}

/**
 * @brief S05：半行
 */
static void test_s05_half_line(void)
{
    const uint8_t *p = NULL;

    (void)printf("\n==== S05 半行无 \\r\\n ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"AT+EC\"\n");
    TEST_CHECK(test_feed_str("AT+EC") == 5);
    TEST_CHECK(lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE) ==
               LW_AT_STREAM_NO_LINE);
    (void)printf("<< peek=%u (数据应仍在)\n",
                 (unsigned)lw_at_stream_peek(&stream, &p));
    TEST_CHECK(lw_at_stream_peek(&stream, &p) == 5U);
}

/**
 * @brief S06：仅 CR
 */
static void test_s06_cr_only(void)
{
    (void)printf("\n==== S06 仅 \\r 无 \\n ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"AT\\r\"\n");
    TEST_CHECK(test_feed_str("AT\r") == 3);
    TEST_CHECK(lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE) ==
               LW_AT_STREAM_NO_LINE);
    (void)printf("<< get_line=NO_LINE\n");
}

/**
 * @brief S07：仅 LF
 */
static void test_s07_lf_only(void)
{
    (void)printf("\n==== S07 仅 \\n 无前导 \\r ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"AT\\n\"\n");
    TEST_CHECK(test_feed_str("AT\n") == 3);
    TEST_CHECK(lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE) ==
               LW_AT_STREAM_NO_LINE);
    (void)printf("<< get_line=NO_LINE\n");
}

/**
 * @brief S08：空行
 */
static void test_s08_empty_line(void)
{
    int32_t n;

    (void)printf("\n==== S08 空行 \\r\\n ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"\\r\\n\"\n");
    TEST_CHECK(test_feed_str("\r\n") == 2);
    n = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< get_line=%ld out=\"%s\"\n", (long)n, out_mem);
    TEST_CHECK(n == 0);
    TEST_CHECK(out_mem[0] == '\0');
}

/**
 * @brief S09：多行粘包
 */
static void test_s09_multi_line(void)
{
    int32_t n1;
    int32_t n2;

    (void)printf("\n==== S09 一次 feed 多行 ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"AT\\r\\nAT+ECHO?\\r\\n\"\n");
    TEST_CHECK(test_feed_str("AT\r\nAT+ECHO?\r\n") == 14);
    n1 = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< line1=%ld \"%s\"\n", (long)n1, out_mem);
    TEST_CHECK(n1 == 2);
    TEST_CHECK(strcmp(out_mem, "AT") == 0);
    n2 = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< line2=%ld \"%s\"\n", (long)n2, out_mem);
    TEST_CHECK(n2 == 8);
    TEST_CHECK(strcmp(out_mem, "AT+ECHO?") == 0);
}

/**
 * @brief S10：行超长
 */
static void test_s10_line_long(void)
{
    char small[TEST_OUT_SMALL];
    int32_t n;

    (void)printf("\n==== S10 行超长（out_size=%u） ====\n",
                 (unsigned)TEST_OUT_SMALL);
    test_stream_reset_fixture();
    (void)printf(">> \"ABCDE\\r\\nAT\\r\\n\"\n");
    TEST_CHECK(test_feed_str("ABCDE\r\nAT\r\n") == 11);
    n = lw_at_stream_get_line(&stream, small, TEST_OUT_SMALL);
    (void)printf("<< first=%ld (expect LINE_LONG=%d)\n", (long)n,
                 (int)LW_AT_STREAM_LINE_LONG);
    TEST_CHECK(n == LW_AT_STREAM_LINE_LONG);
    n = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< second=%ld \"%s\"\n", (long)n, out_mem);
    TEST_CHECK(n == 2);
    TEST_CHECK(strcmp(out_mem, "AT") == 0);
}

/**
 * @brief S11：环形回绕取行
 */
static void test_s11_wrap_line(void)
{
    char pad[TEST_RX_SIZE];
    int32_t n;
    uint32_t cap = TEST_RX_SIZE - 1U;

    (void)printf("\n==== S11 环形回绕后取行 ====\n");
    test_stream_reset_fixture();
    memset(pad, (int)'X', sizeof(pad));
    /* 先写满再取出一行占用，使 head 回绕 */
    (void)printf(">> 填满 %u 字节后取空，再写 \"OK\\r\\n\"\n", (unsigned)cap);
    TEST_CHECK(lw_at_stream_feed(&stream, (const uint8_t *)pad, cap) ==
               (int32_t)cap);
    lw_at_stream_reset(&stream);
    TEST_CHECK(test_feed_str("OK\r\n") == 4);
    n = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< get_line=%ld out=\"%s\"\n", (long)n, out_mem);
    TEST_CHECK(n == 2);
    TEST_CHECK(strcmp(out_mem, "OK") == 0);
}

/**
 * @brief S12：写满溢出
 */
static void test_s12_overflow(void)
{
    char junk[TEST_RX_SIZE + 8U];
    uint32_t cap = TEST_RX_SIZE - 1U;
    int32_t w;

    (void)printf("\n==== S12 缓存写满 ====\n");
    test_stream_reset_fixture();
    memset(junk, (int)'Z', sizeof(junk));
    (void)printf(">> feed %u 字节到 size=%u 缓存\n", (unsigned)sizeof(junk),
                 (unsigned)TEST_RX_SIZE);
    w = lw_at_stream_feed(&stream, (const uint8_t *)junk, (uint32_t)sizeof(junk));
    (void)printf("<< written=%ld overflow=%u\n", (long)w,
                 (unsigned)stream.overflow);
    TEST_CHECK(w == (int32_t)cap);
    TEST_CHECK(lw_at_stream_overflow_take(&stream) == 1U);
    TEST_CHECK(lw_at_stream_overflow_take(&stream) == 0U);
}

/**
 * @brief S13：溢出后仍可取出溢出前已完整的行
 */
static void test_s13_overflow_keeps_line(void)
{
    char junk[TEST_RX_SIZE];
    int32_t n;

    (void)printf("\n==== S13 溢出后仍可取已有完整行（模块语义） ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"AT\\r\\n\" + 超量填充\n");
    TEST_CHECK(test_feed_str("AT\r\n") == 4);
    memset(junk, (int)'Q', sizeof(junk));
    (void)lw_at_stream_feed(&stream, (const uint8_t *)junk, (uint32_t)sizeof(junk));
    TEST_CHECK(stream.overflow == 1U);
    n = lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE);
    (void)printf("<< get_line=%ld out=\"%s\" overflow_take=%u\n", (long)n, out_mem,
                 (unsigned)lw_at_stream_overflow_take(&stream));
    TEST_CHECK(n == 2);
    TEST_CHECK(strcmp(out_mem, "AT") == 0);
}

/**
 * @brief S14：reset
 */
static void test_s14_reset(void)
{
    const uint8_t *p = NULL;

    (void)printf("\n==== S14 reset 丢弃未读 ====\n");
    test_stream_reset_fixture();
    (void)printf(">> \"HALF\" then reset\n");
    TEST_CHECK(test_feed_str("HALF") == 4);
    lw_at_stream_reset(&stream);
    (void)printf("<< peek=%u\n", (unsigned)lw_at_stream_peek(&stream, &p));
    TEST_CHECK(lw_at_stream_peek(&stream, &p) == 0U);
    TEST_CHECK(lw_at_stream_get_line(&stream, out_mem, TEST_OUT_SIZE) ==
               LW_AT_STREAM_NO_LINE);
}

/**
 * @brief S15：peek/consume 回绕分段
 */
static void test_s15_peek_wrap(void)
{
    char pad[TEST_RX_SIZE];
    const uint8_t *p = NULL;
    uint32_t n1;
    uint32_t n2;
    uint32_t cap = TEST_RX_SIZE - 1U;

    (void)printf("\n==== S15 peek/consume 回绕分段 ====\n");
    test_stream_reset_fixture();
    memset(pad, (int)'A', sizeof(pad));
    /* 写满后消费 1 字节，再写 2 字节，制造 head < tail 的回绕可读布局：
       实际：reset 后写 (cap-1) 个再写会... 更简单：
       填满 -> reset 清空 -> 写入使数据跨界：
       先写 size-2 字节，consume 全部，再写 3 字节从末尾附近开始 */
    TEST_CHECK(lw_at_stream_feed(&stream, (const uint8_t *)pad, cap) ==
               (int32_t)cap);
    lw_at_stream_consume(&stream, cap - 2U);
    /* 此时剩余 2 字节在物理末尾附近，再写入 4 字节会回绕到开头 */
    TEST_CHECK(lw_at_stream_feed(&stream, (const uint8_t *)"WXYZ", 4U) == 4);
    n1 = lw_at_stream_peek(&stream, &p);
    (void)printf("<< peek1=%u (应到物理末尾一段)\n", (unsigned)n1);
    TEST_CHECK(n1 > 0U);
    TEST_CHECK(n1 < 6U);
    lw_at_stream_consume(&stream, n1);
    n2 = lw_at_stream_peek(&stream, &p);
    (void)printf("<< peek2=%u\n", (unsigned)n2);
    TEST_CHECK(n2 > 0U);
    TEST_CHECK((n1 + n2) == 6U);
}

/**
 * @brief S16：pending 标志一次性语义（由定时器回调置位，take 清零）
 */
static void test_s16_pending_take(void)
{
    uint8_t v;

    (void)printf("\n==== S16 pending_take 一次性 ====\n");
    test_stream_reset_fixture();
    stream.pending = 1U;
    v = lw_at_stream_pending_take(&stream);
    (void)printf("<< pending_take=%u\n", (unsigned)v);
    TEST_CHECK(v == 1U);
    TEST_CHECK(lw_at_stream_pending_take(&stream) == 0U);
}

/**
 * @brief S17：overflow_take（与 S12 呼应的清零语义，独立用例）
 */
static void test_s17_overflow_take(void)
{
    char one = 'X';
    uint32_t i;
    uint32_t cap = TEST_RX_SIZE - 1U;

    (void)printf("\n==== S17 overflow_take 清零 ====\n");
    test_stream_reset_fixture();
    for (i = 0U; i < cap; i++) {
        TEST_CHECK(lw_at_stream_feed(&stream, (const uint8_t *)&one, 1U) == 1);
    }
    TEST_CHECK(lw_at_stream_feed(&stream, (const uint8_t *)&one, 1U) == 0);
    TEST_CHECK(lw_at_stream_overflow_take(&stream) == 1U);
    TEST_CHECK(lw_at_stream_overflow_take(&stream) == 0U);
    (void)printf("<< overflow take once then zero\n");
}

/**
 * @brief 入口
 * @return 0 全过；1 有失败
 */
int main(void)
{
    (void)printf("lw_at_stream unit test\n");
    (void)printf("see TEST.md for items / reasons / expected results\n");

    test_s01_init_empty();
    test_s02_full_line();
    test_s03_byte_fragment();
    test_s04_arb_fragment();
    test_s05_half_line();
    test_s06_cr_only();
    test_s07_lf_only();
    test_s08_empty_line();
    test_s09_multi_line();
    test_s10_line_long();
    test_s11_wrap_line();
    test_s12_overflow();
    test_s13_overflow_keeps_line();
    test_s14_reset();
    test_s15_peek_wrap();
    test_s16_pending_take();
    test_s17_overflow_take();

    (void)printf("\n==== 汇总 ====\n");
    (void)printf("checks: %u, failed: %u\n", (unsigned)check_total,
                 (unsigned)check_fail);
    return (check_fail == 0U) ? 0 : 1;
}
