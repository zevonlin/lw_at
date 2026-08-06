/**
 * @file lw_at_port_win.c
 * @brief LW-AT Windows 例程适配层实现（非库核心）
 *
 * @details
 * write 经 fwrite 打到 stdout；timer_arm 使用 Windows 线程池定时器
 * CreateTimerQueueTimer，到期后由线程池回调直接触发 cb，无需主循环
 * 轮询。模拟 MCU 上 UART 发送 + 单次硬件定时器的最小形态。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-06
 * @version 0.9.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                              version
 * 2026-08-06 linzhiwei 首次发布                            v0.9.0
 */
#include "lw_at_port_win.h"

#include <stdio.h>
#include <windows.h>

static HANDLE          g_timer      = NULL;
static lw_at_timer_cb_t g_timer_cb  = NULL;
static void           *g_timer_user = NULL;

/**
 * @brief Windows 线程池定时器到期回调，转调库注册的 lw_at_timer_cb_t
 * @param param  未使用
 * @param fired 未使用
 */
static VOID CALLBACK port_win_timer_callback(PVOID param, BOOLEAN fired)
{
    lw_at_timer_cb_t cb;
    void *user;

    (void)param;
    (void)fired;

    cb   = g_timer_cb;
    user = g_timer_user;
    if (cb != NULL) {
        cb(user);
    }
}

/**
 * @brief 写出从机应答到控制台
 */
int32_t port_win_write(const uint8_t *data, int32_t len)
{
    size_t n;

    if ((data == NULL) || (len <= 0)) {
        return 0;
    }
    n = fwrite(data, 1U, (size_t)len, stdout);
    (void)fflush(stdout);
    return (int32_t)n;
}

/**
 * @brief 启动/重载单次软件定时器
 */
int32_t port_win_timer_arm(uint32_t ms, lw_at_timer_cb_t cb, void *user)
{
    /* 先取消旧定时器，防止泄漏 */
    if (g_timer != NULL) {
        (void)DeleteTimerQueueTimer(NULL, g_timer, INVALID_HANDLE_VALUE);
        g_timer = NULL;
    }

    g_timer_cb   = cb;
    g_timer_user = user;

    /* Period=0 即单次；WT_EXECUTEDEFAULT 让线程池决定执行方式 */
    if (!CreateTimerQueueTimer(&g_timer, NULL,
                               port_win_timer_callback, NULL,
                               ms,    /* DueTime */
                               0U,    /* Period = 0 = 单次 */
                               WT_EXECUTEDEFAULT)) {
        g_timer_cb = NULL;
        return -1;
    }
    return 0;
}

/**
 * @brief 停止定时器并等待当前回调完成
 */
void port_win_timer_stop(void)
{
    if (g_timer != NULL) {
        /* INVALID_HANDLE_VALUE = 阻塞等待回调执行完毕 */
        (void)DeleteTimerQueueTimer(NULL, g_timer, INVALID_HANDLE_VALUE);
        g_timer = NULL;
    }
    g_timer_cb = NULL;
}
