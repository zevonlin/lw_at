/**
 * @file harness.c
 * @brief 异步事件脚本测试框架实现（非库核心）
 *
 * @details
 * 按步骤表调用 lw_at_feed / process，并用 test_port 输出做期望核对。
 * H_TICK 步骤推进虚拟时钟并触发到期的单次定时器回调。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 1.2.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei TICK 改为推进虚拟时钟触发定时器回调          v1.2.0
 * 2026-07-30 linzhiwei 增加 H_EXPECT_OUT_COUNT                    v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
 */
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "lw_at.h"
#include "test_port.h"

static uint32_t check_total;
static uint32_t check_fail;

/**
 * @brief 断言并打印
 * @param ok   非 0 通过
 * @param msg  说明
 * @param line 行号
 */
static void h_check(int ok, const char *msg, int line)
{
    check_total++;
    if (ok != 0) {
        (void)printf("[PASS] %s\n", msg);
    } else {
        check_fail++;
        (void)printf("[FAIL] line %d: %s\n", line, msg);
        (void)printf("       got \"%s\"\n", test_port_out_get());
    }
}

#define H_CHECK(cond, msg) h_check((cond) ? 1 : 0, (msg), __LINE__)

/**
 * @brief 统计子串出现次数（不重叠）
 * @param hay  被搜索串
 * @param needle 子串；空串视为 0 次
 * @return 出现次数
 */
static uint32_t h_count_substr(const char *hay, const char *needle)
{
    uint32_t count = 0U;
    size_t nlen;

    if ((hay == NULL) || (needle == NULL) || (needle[0] == '\0')) {
        return 0U;
    }
    nlen = strlen(needle);
    while ((hay = strstr(hay, needle)) != NULL) {
        count++;
        hay += nlen;
    }
    return count;
}

/**
 * @brief 转义打印一段字节（最多打印前若干字符）
 * @param data 数据
 * @param len  长度
 */
static void h_print_feed(const uint8_t *data, uint32_t len)
{
    uint32_t i;

    (void)printf(">> FEED[%u] \"", (unsigned)len);
    for (i = 0U; i < len; i++) {
        uint8_t b = data[i];

        if (b == (uint8_t)'\r') {
            (void)printf("\\r");
        } else if (b == (uint8_t)'\n') {
            (void)printf("\\n");
        } else if ((b < 0x20U) || (b >= 0x7FU)) {
            (void)printf("\\x%02X", (unsigned)b);
        } else {
            (void)putchar((int)b);
        }
    }
    (void)printf("\"\n");
}

uint32_t harness_fail_count(void)
{
    return check_fail;
}

uint32_t harness_check_count(void)
{
    return check_total;
}

/**
 * @brief 运行一条事件脚本
 */
int harness_run(const char *name, h_setup_fn setup, const h_step_t *steps)
{
    const h_step_t *step;
    int scenario_fail = 0;
    uint32_t fail_before = check_fail;

    (void)printf("\n==== %s ====\n", name);
    if (setup != NULL) {
        setup();
    }

    for (step = steps; step->type != H_END; step++) {
        switch (step->type) {
        case H_INFO:
            (void)printf("-- %s\n", (const char *)step->p);
            break;

        case H_FEED: {
            const char *s = (const char *)step->p;
            uint32_t n = (uint32_t)strlen(s);

            h_print_feed((const uint8_t *)s, n);
            (void)lw_at_feed((const uint8_t *)s, n);
            break;
        }

        case H_FEED_BIN:
            h_print_feed((const uint8_t *)step->p, step->u);
            (void)lw_at_feed((const uint8_t *)step->p, step->u);
            break;

        case H_TICK:
            (void)printf(">> TICK +%u ms\n", (unsigned)step->u);
            /* 推进虚拟时钟；到期的单次定时器回调在此触发（等价硬件定时器 ISR） */
            test_port_tick_advance(step->u);
            break;

        case H_PROCESS:
            (void)printf(">> PROCESS\n");
            lw_at_process();
            (void)printf("<< \"%s\"\n", test_port_out_get());
            break;

        case H_OUT_CLEAR:
            test_port_out_clear();
            break;

        case H_EXPECT_OUT: {
            const char *exp = (const char *)step->p;
            const char *got = test_port_out_get();

            H_CHECK(strcmp(got, exp) == 0, "EXPECT_OUT exact match");
            break;
        }

        case H_EXPECT_OUT_HAS: {
            const char *sub = (const char *)step->p;
            const char *got = test_port_out_get();

            H_CHECK(strstr(got, sub) != NULL, "EXPECT_OUT_HAS substring");
            break;
        }

        case H_EXPECT_OUT_COUNT: {
            const char *sub = (const char *)step->p;
            const char *got = test_port_out_get();
            uint32_t got_n = h_count_substr(got, sub);

            if (got_n != step->u) {
                (void)printf("       count got=%u expect=%u sub=\"%s\"\n",
                             (unsigned)got_n, (unsigned)step->u, sub);
            }
            H_CHECK(got_n == step->u, "EXPECT_OUT_COUNT");
            break;
        }

        case H_EXPECT_FN: {
            int (*fn)(void) = (int (*)(void))step->p;

            H_CHECK((fn != NULL) && (fn() != 0), "EXPECT_FN custom");
            break;
        }

        default:
            H_CHECK(0, "unknown event type");
            break;
        }
    }

    if (check_fail > fail_before) {
        scenario_fail = 1;
        (void)printf("** scenario FAILED **\n");
    } else {
        (void)printf("** scenario OK **\n");
    }
    return scenario_fail;
}
