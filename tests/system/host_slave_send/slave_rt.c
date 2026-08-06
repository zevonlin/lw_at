/**
 * @file slave_rt.c
 * @brief host_slave_send 从机运行时实现
 *
 * @details
 * CreateThread 循环：读下行 feed、tick、process；处理 data_exit 请求。
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
 * 2026-08-01 linzhiwei 事件驱动：timer_arm/stop + 线程内轮询触发   v1.1.0
 * 2026-07-31 linzhiwei 首次发布                                    v1.0.0
 */
#include "slave_rt.h"

#include "lw_at.h"
#include "send_cmd.h"

#include <string.h>
#include <windows.h>

#define SLAVE_RX_DEFAULT 2048U
#define SLAVE_LINE_DEFAULT 128U
#define SLAVE_TX_SIZE 256U
#define SLAVE_FEED_CHUNK 128U
#define SLAVE_LOOP_SLEEP_MS 1U
#define SLAVE_SINK_CAP 4096U
#define SLAVE_RX_MAX 4096U
#define SLAVE_LINE_MAX 256U
#define SLAVE_IDLE_DEFAULT 40U
#define SLAVE_GUARD_DEFAULT 80U

static link_q_t *down_q;
static link_q_t *up_q;
static volatile LONG slave_run;
static volatile LONG exit_req_flag;
static HANDLE slave_thread;
static uint8_t rx_mem[SLAVE_RX_MAX];
static uint8_t line_mem[SLAVE_LINE_MAX];
static uint8_t tx_mem[SLAVE_TX_SIZE];
static lw_at_config_t cfg;
static slave_rt_cfg_t active_cfg;
static char sink_buf[SLAVE_SINK_CAP];
static uint32_t sink_len;
static CRITICAL_SECTION sink_lock;
static int sink_lock_inited;

/**
 * @brief Port write → 上行
 * @param data 数据
 * @param len  长度
 * @return 写入数
 */
static int32_t slave_port_write(const uint8_t *data, int32_t len)
{
    if ((data == NULL) || (len <= 0) || (up_q == NULL)) {
        return (int32_t)LW_AT_ERR_PARAM;
    }
    return (int32_t)link_q_write(up_q, data, (uint32_t)len);
}

/**
 * @brief 墙钟毫秒（供虚拟定时器 deadline 使用，仅从机线程读取）
 * @return GetTickCount
 */
static uint32_t slave_port_tick(void)
{
    return (uint32_t)GetTickCount();
}

/* 单次定时器登记状态：是否已 arm */
static volatile uint8_t timer_armed;

/* 单次定时器到期时刻（ms） */
static volatile uint32_t timer_deadline;

/* 到期回调（库内部函数） */
static lw_at_timer_cb_t timer_cb;

/* 到期回调用户指针 */
static void *timer_user;

/**
 * @brief Port timer_arm：登记单次定时器，由从机线程轮询触发
 * @param ms   定时时长
 * @param cb   到期回调
 * @param user 用户指针
 * @return 0
 */
static int32_t slave_port_timer_arm(uint32_t ms, lw_at_timer_cb_t cb, void *user)
{
    timer_deadline = slave_port_tick() + ms;
    timer_armed = 1U;
    timer_cb = cb;
    timer_user = user;
    return 0;
}

/**
 * @brief Port timer_stop：停止定时器
 */
static void slave_port_timer_stop(void)
{
    timer_armed = 0U;
    timer_cb = NULL;
    timer_user = NULL;
}

/**
 * @brief 从机线程内检查定时器到期并触发回调（模拟硬件定时器 ISR）
 */
static void slave_port_timer_service(void)
{
    if ((timer_armed != 0U) && (slave_port_tick() >= timer_deadline)) {
        lw_at_timer_cb_t cb = timer_cb;
        void *user = timer_user;

        timer_armed = 0U;
        if (cb != NULL) {
            cb(user);
        }
    }
}

/**
 * @brief 流式 sink
 */
static void slave_sink(const uint8_t *data, uint32_t len, void *user)
{
    uint32_t i;

    (void)user;
    EnterCriticalSection(&sink_lock);
    for (i = 0U; i < len; i++) {
        if (sink_len < (SLAVE_SINK_CAP - 1U)) {
            sink_buf[sink_len] = (char)data[i];
            sink_len++;
        }
    }
    sink_buf[sink_len] = '\0';
    LeaveCriticalSection(&sink_lock);
}

/**
 * @brief 从机线程
 * @param arg 未使用
 * @return 0
 */
static DWORD WINAPI slave_thread_main(LPVOID arg)
{
    uint8_t chunk[SLAVE_FEED_CHUNK];

    (void)arg;
    while (InterlockedCompareExchange(&slave_run, 1, 1) == 1) {
        if (InterlockedExchange(&exit_req_flag, 0) == 1) {
            (void)lw_at_data_exit();
        }
        for (;;) {
            uint32_t n = link_q_read(down_q, chunk, sizeof(chunk));

            if (n == 0U) {
                break;
            }
            (void)lw_at_feed(chunk, n);
        }
        slave_port_timer_service();
        lw_at_process();
        Sleep(SLAVE_LOOP_SLEEP_MS);
    }
    return 0;
}

/**
 * @brief 应用配置并 init
 * @param c 配置
 * @return 0 成功
 */
static int slave_rt_apply(const slave_rt_cfg_t *c)
{
    uint32_t rx_n = SLAVE_RX_DEFAULT;
    uint32_t line_n = SLAVE_LINE_DEFAULT;
    uint32_t idle = SLAVE_IDLE_DEFAULT;
    uint32_t guard = SLAVE_GUARD_DEFAULT;

    if (c != NULL) {
        if (c->idle_ms > 0U) {
            idle = c->idle_ms;
        }
        if (c->rx_buf_size > 0U) {
            rx_n = c->rx_buf_size;
        }
        if (c->line_buf_size > 0U) {
            line_n = c->line_buf_size;
        }
        if (c->guard_ms > 0U) {
            guard = c->guard_ms;
        }
    }
    if (rx_n > SLAVE_RX_MAX) {
        rx_n = SLAVE_RX_MAX;
    }
    if (line_n > SLAVE_LINE_MAX) {
        line_n = SLAVE_LINE_MAX;
    }
    if (rx_n < 2U) {
        rx_n = 2U;
    }

    active_cfg.idle_ms = idle;
    active_cfg.rx_buf_size = rx_n;
    active_cfg.line_buf_size = line_n;
    active_cfg.guard_ms = guard;

    lw_at_deinit();
    memset(&cfg, 0, sizeof(cfg));
    cfg.rx_buf = rx_mem;
    cfg.rx_buf_size = rx_n;
    cfg.line_buf = line_mem;
    cfg.line_buf_size = line_n;
    cfg.tx_buf = tx_mem;
    cfg.tx_buf_size = SLAVE_TX_SIZE;
    cfg.port.write = slave_port_write;
    cfg.port.timer_arm = slave_port_timer_arm;
    cfg.port.timer_stop = slave_port_timer_stop;
    cfg.idle_timeout_ms = idle;
    cfg.guard_ms = guard;
    cfg.cbs.sink = slave_sink;

    if (lw_at_init(&cfg) != LW_AT_ERR_OK) {
        return -1;
    }
    if (send_cmd_register() != LW_AT_ERR_OK) {
        lw_at_deinit();
        return -1;
    }
    send_cmd_reset_state();
    InterlockedExchange(&exit_req_flag, 0);
    return 0;
}

int slave_rt_start(link_q_t *down, link_q_t *up, const slave_rt_cfg_t *cfg_in)
{
    down_q = down;
    up_q = up;
    if (sink_lock_inited == 0) {
        InitializeCriticalSection(&sink_lock);
        sink_lock_inited = 1;
    }
    slave_rt_sink_clear();

    if (slave_rt_apply(cfg_in) != 0) {
        return -1;
    }

    InterlockedExchange(&slave_run, 1);
    slave_thread = CreateThread(NULL, 0, slave_thread_main, NULL, 0, NULL);
    if (slave_thread == NULL) {
        InterlockedExchange(&slave_run, 0);
        lw_at_deinit();
        return -1;
    }
    return 0;
}

void slave_rt_stop(void)
{
    InterlockedExchange(&slave_run, 0);
    if (slave_thread != NULL) {
        (void)WaitForSingleObject(slave_thread, 5000);
        CloseHandle(slave_thread);
        slave_thread = NULL;
    }
    lw_at_deinit();
}

int slave_rt_reinit(const slave_rt_cfg_t *cfg_in)
{
    InterlockedExchange(&slave_run, 0);
    if (slave_thread != NULL) {
        (void)WaitForSingleObject(slave_thread, 5000);
        CloseHandle(slave_thread);
        slave_thread = NULL;
    }
    if (slave_rt_apply(cfg_in) != 0) {
        return -1;
    }
    InterlockedExchange(&slave_run, 1);
    slave_thread = CreateThread(NULL, 0, slave_thread_main, NULL, 0, NULL);
    if (slave_thread == NULL) {
        InterlockedExchange(&slave_run, 0);
        lw_at_deinit();
        return -1;
    }
    return 0;
}

const char *slave_rt_sink_get(void)
{
    return sink_buf;
}

uint32_t slave_rt_sink_len(void)
{
    uint32_t n;

    EnterCriticalSection(&sink_lock);
    n = sink_len;
    LeaveCriticalSection(&sink_lock);
    return n;
}

void slave_rt_sink_clear(void)
{
    if (sink_lock_inited == 0) {
        return;
    }
    EnterCriticalSection(&sink_lock);
    sink_len = 0U;
    sink_buf[0] = '\0';
    LeaveCriticalSection(&sink_lock);
}

void slave_rt_request_data_exit(void)
{
    InterlockedExchange(&exit_req_flag, 1);
}

uint32_t slave_rt_guard_ms(void)
{
    return active_cfg.guard_ms;
}

uint32_t slave_rt_idle_ms(void)
{
    return active_cfg.idle_ms;
}
