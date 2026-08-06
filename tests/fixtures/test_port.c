/**
 * @file test_port.c
 * @brief 测试用 Port：应答捕获与虚拟单次软件定时器实现
 *
 * @details
 * 捕获缓冲写满后丢弃多余输出（仅影响观测，不影响被测库行为）。
 * 定时器为虚拟单次实现：tick_advance 推进时钟并在到期时调用回调，
 * 模拟硬件定时器 ISR；重复 arm 覆盖前次，stop 清除。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 3.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei 事件驱动改造：移除 get_tick_ms，新增 timer   v3.0.0
 * 2026-07-30 linzhiwei 由 port_pc 重命名为 test_port               v2.0.0
 * 2026-07-30 linzhiwei 迁入 tests/fixtures                         v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
 */
#include "test_port.h"

/* 从机上行输出捕获缓冲，末字节预留给 NUL，供测试比对应答 */
static char out_buf[TEST_PORT_OUT_BUF_SIZE];

/* 捕获缓冲当前有效字节数 */
static uint32_t out_len;

/* 虚拟毫秒时钟当前值，由 test_port_tick_advance 推进 */
static uint32_t tick_now;

/* 单次定时器登记状态：是否已 arm */
static uint8_t timer_armed;

/* 单次定时器到期时刻（ms） */
static uint32_t timer_deadline;

/* 到期回调（库内部函数） */
static lw_at_timer_cb_t timer_cb;

/* 到期回调用户指针 */
static void *timer_user;

/**
 * @brief 追加从机上行到捕获缓冲
 */
int32_t test_port_write(const uint8_t *data, int32_t len)
{
    int32_t i;

    for (i = 0; i < len; i++) {
        if (out_len < (TEST_PORT_OUT_BUF_SIZE - 1U)) {
            out_buf[out_len] = (char)data[i];
            out_len++;
        }
    }
    return len;
}

/**
 * @brief 启动/重载单次软件定时器
 */
int32_t test_port_timer_arm(uint32_t ms, lw_at_timer_cb_t cb, void *user)
{
    timer_deadline = tick_now + ms;
    timer_armed = 1U;
    timer_cb = cb;
    timer_user = user;
    return 0;
}

/**
 * @brief 停止定时器
 */
void test_port_timer_stop(void)
{
    timer_armed = 0U;
    timer_cb = NULL;
    timer_user = NULL;
}

/**
 * @brief 推进虚拟时钟并触发到期的单次定时器
 */
void test_port_tick_advance(uint32_t ms)
{
    tick_now += ms;
    if ((timer_armed != 0U) && (tick_now >= timer_deadline)) {
        lw_at_timer_cb_t cb = timer_cb;
        void *user = timer_user;

        timer_armed = 0U;
        if (cb != NULL) {
            cb(user);
        }
    }
}

/**
 * @brief 读取当前捕获的全部输出
 */
const char *test_port_out_get(void)
{
    out_buf[out_len] = '\0';
    return out_buf;
}

/**
 * @brief 清空输出捕获缓冲
 */
void test_port_out_clear(void)
{
    out_len = 0U;
}
