/**
 * @file host_api.c
 * @brief 主机侧 API 实现
 *
 * @details
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 1.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                              version
 * 2026-08-11 linzhiwei 新增 host_collect_impaired 上行损伤 v1.1.0
 * 2026-07-30 linzhiwei 首次发布 v1.0.0
 */
#include "host_api.h"

#include "hs_cmd.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static link_q_t *down_q;
static link_q_t *up_q;
static uint32_t idle_ms = 50U;
static uint32_t check_total;
static uint32_t check_fail;

void host_api_bind(link_q_t *down, link_q_t *up, uint32_t idle)
{
    down_q = down;
    up_q = up;
    if (idle > 0U) {
        idle_ms = idle;
    }
}

uint32_t host_settle_ms(void)
{
    return idle_ms + 100U;
}

void host_scene_begin(void)
{
    uint8_t trash[256];

    /* 吸干残留上下行，避免场景串扰 */
    while (link_q_read(up_q, trash, sizeof(trash)) > 0U) {
    }
    while (link_q_read(down_q, trash, sizeof(trash)) > 0U) {
    }
    link_q_reset(down_q);
    link_q_reset(up_q);
    hs_cmd_reset_state();
    Sleep(20);
}

void host_send(const char *s)
{
    uint32_t n;

    if ((s == NULL) || (down_q == NULL)) {
        return;
    }
    n = (uint32_t)strlen(s);
    (void)printf("HOST>> [%u] ", (unsigned)n);
    {
        uint32_t i;
        (void)printf("\"");
        for (i = 0U; i < n; i++) {
            char c = s[i];
            if (c == '\r') {
                (void)printf("\\r");
            } else if (c == '\n') {
                (void)printf("\\n");
            } else {
                (void)putchar((int)c);
            }
        }
        (void)printf("\"\n");
    }
    (void)link_q_write(down_q, (const uint8_t *)s, n);
}

void host_send_bin(const uint8_t *data, uint32_t len)
{
    (void)printf("HOST>> BIN[%u]\n", (unsigned)len);
    (void)link_q_write(down_q, data, len);
}

void host_send_impaired(const char *s, const link_impair_t *impair)
{
    uint32_t n;

    if (s == NULL) {
        return;
    }
    n = (uint32_t)strlen(s);
    (void)printf("HOST>> IMPAIR[%u]\n", (unsigned)n);
    (void)link_impair_write(down_q, impair, (const uint8_t *)s, n);
}

uint32_t host_collect(char *out, uint32_t out_cap, uint32_t wait_ms)
{
    uint32_t got = 0U;
    DWORD t0 = GetTickCount();

    if ((out == NULL) || (out_cap == 0U)) {
        return 0U;
    }
    out[0] = '\0';

    /* 直到总超时：中间短静默不提前结束（压力连回多段 OK 需要） */
    while ((GetTickCount() - t0) < wait_ms) {
        uint8_t chunk[128];
        uint32_t n;
        uint32_t space;
        DWORD left = wait_ms - (GetTickCount() - t0);
        DWORD slice = (left > 40U) ? 40U : left;

        if (got + 1U >= out_cap) {
            break;
        }
        if (slice == 0U) {
            break;
        }
        space = out_cap - 1U - got;
        if (space > sizeof(chunk)) {
            space = (uint32_t)sizeof(chunk);
        }
        n = link_q_read_wait(up_q, chunk, space, slice);
        if (n > 0U) {
            memcpy(out + got, chunk, n);
            got += n;
            out[got] = '\0';
        }
    }
    return got;
}

uint32_t host_collect_settle(char *out, uint32_t out_cap)
{
    return host_collect(out, out_cap, host_settle_ms());
}

uint32_t host_collect_impaired(char *out, uint32_t out_cap, uint32_t wait_ms,
                               uint32_t impair_every)
{
    uint32_t got = 0U;
    uint32_t byte_index = 0U;
    DWORD t0 = GetTickCount();

    if ((out == NULL) || (out_cap == 0U)) {
        return 0U;
    }
    out[0] = '\0';

    while ((GetTickCount() - t0) < wait_ms) {
        uint8_t chunk[128];
        uint32_t n;
        uint32_t space;
        uint32_t i;
        DWORD left = wait_ms - (GetTickCount() - t0);
        DWORD slice = (left > 40U) ? 40U : left;

        if (got + 1U >= out_cap) {
            break;
        }
        if (slice == 0U) {
            break;
        }
        space = out_cap - 1U - got;
        if (space > sizeof(chunk)) {
            space = (uint32_t)sizeof(chunk);
        }
        n = link_q_read_wait(up_q, chunk, space, slice);
        if (n == 0U) {
            continue;
        }
        /* 逐字节施加损伤：每 N 字节丢 1 个 + 插入 1 个噪声字节 */
        for (i = 0U; (i < n) && (got + 1U < out_cap); i++) {
            byte_index++;
            if ((impair_every > 0U) && ((byte_index % impair_every) == 0U)) {
                /* 丢弃本字节，替换为噪声字节 */
                out[got++] = (char)0x00;
            } else {
                out[got++] = (char)chunk[i];
            }
        }
        out[got] = '\0';
    }
    return got;
}

uint32_t host_count(const char *hay, const char *needle)
{
    uint32_t n = 0U;
    size_t len;

    if ((hay == NULL) || (needle == NULL) || (needle[0] == '\0')) {
        return 0U;
    }
    len = strlen(needle);
    while ((hay = strstr(hay, needle)) != NULL) {
        n++;
        hay += len;
    }
    return n;
}

void host_check(int ok, const char *msg)
{
    check_total++;
    if (ok != 0) {
        (void)printf("[PASS] %s\n", msg);
    } else {
        check_fail++;
        (void)printf("[FAIL] %s\n", msg);
    }
}

void host_check_stats(uint32_t *total, uint32_t *fail)
{
    if (total != NULL) {
        *total = check_total;
    }
    if (fail != NULL) {
        *fail = check_fail;
    }
}
