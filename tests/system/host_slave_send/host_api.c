/**
 * @file host_api.c
 * @brief host_slave_send 主机侧 API 实现
 *
 * @details
 * 日志用中文标注【主机→从机】/【从机→主机】，便于区分双端交互。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 1.3.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                              version
 * 2026-08-11 linzhiwei 新增 host_exit_stream_ok 消费退出回包 v1.3.0
 * 2026-07-31 linzhiwei 中文双端日志与控制台 UTF-8         v1.2.0
 * 2026-07-31 linzhiwei 精确等待/安静窗/子串计数           v1.1.0
 * 2026-07-31 linzhiwei 首次发布                            v1.0.0
 */
#include "host_api.h"

#include "send_cmd.h"
#include "slave_rt.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

static link_q_t *down_q;
static link_q_t *up_q;
static uint32_t idle_ms = 40U;
static uint32_t check_total;
static uint32_t check_fail;

/**
 * @brief 转义打印一段字节（\\r \\n 与可打印字符）
 * @param data 数据
 * @param len  长度
 */
static void host_print_escaped(const uint8_t *data, uint32_t len)
{
    uint32_t i;

    (void)printf("\"");
    for (i = 0U; i < len; i++) {
        uint8_t c = data[i];

        if (c == (uint8_t)'\r') {
            (void)printf("\\r");
        } else if (c == (uint8_t)'\n') {
            (void)printf("\\n");
        } else if ((c >= 0x20U) && (c <= 0x7EU)) {
            (void)putchar((int)c);
        } else {
            (void)printf("\\x%02X", (unsigned)c);
        }
    }
    (void)printf("\"");
}

/**
 * @brief 打印从机→主机应答
 * @param label 说明
 * @param text  文本（可为二进制经 \\x 转义）
 * @param len   长度；若为 0 则用 strlen(text)
 */
static void host_log_slave(const char *label, const char *text, uint32_t len)
{
    if (text == NULL) {
        return;
    }
    if (len == 0U) {
        len = (uint32_t)strlen(text);
    }
    (void)printf("【从机→主机】%s [%u] ", label, (unsigned)len);
    host_print_escaped((const uint8_t *)text, len);
    (void)printf("\n");
}

/**
 * @brief 打印主机→从机发送
 * @param label 说明
 * @param data  数据
 * @param len   长度
 */
static void host_log_host(const char *label, const uint8_t *data, uint32_t len)
{
    (void)printf("【主机→从机】%s [%u] ", label, (unsigned)len);
    host_print_escaped(data, len);
    (void)printf("\n");
}

void host_api_bind(link_q_t *down, link_q_t *up, uint32_t idle)
{
    down_q = down;
    up_q = up;
    if (idle > 0U) {
        idle_ms = idle;
    }
    /* 控制台按 UTF-8 输出，避免中文乱码 */
    (void)SetConsoleOutputCP(65001);
    (void)SetConsoleCP(65001);
}

uint32_t host_settle_ms(void)
{
    return idle_ms + 120U;
}

void host_scene_begin(void)
{
    uint8_t trash[256];

    while (link_q_read(up_q, trash, sizeof(trash)) > 0U) {
    }
    while (link_q_read(down_q, trash, sizeof(trash)) > 0U) {
    }
    link_q_reset(down_q);
    link_q_reset(up_q);
    send_cmd_reset_state();
    slave_rt_sink_clear();
    Sleep(30);
}

void host_send(const char *s)
{
    uint32_t n;

    if ((s == NULL) || (down_q == NULL)) {
        return;
    }
    n = (uint32_t)strlen(s);
    host_log_host("发送", (const uint8_t *)s, n);
    (void)link_q_write(down_q, (const uint8_t *)s, n);
}

void host_send_bin(const uint8_t *data, uint32_t len)
{
    host_log_host("发送二进制", data, len);
    (void)link_q_write(down_q, data, len);
}

uint32_t host_collect(char *out, uint32_t out_cap, uint32_t wait_ms)
{
    uint32_t got = 0U;
    DWORD t0 = GetTickCount();

    if ((out == NULL) || (out_cap == 0U)) {
        return 0U;
    }
    out[0] = '\0';

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
    if (got > 0U) {
        host_log_slave("应答", out, got);
    } else {
        (void)printf("【从机→主机】应答 [0] （超时无数据）\n");
    }
    return got;
}

uint32_t host_collect_settle(char *out, uint32_t out_cap)
{
    return host_collect(out, out_cap, host_settle_ms());
}

uint32_t host_wait_contains(char *out, uint32_t out_cap, const char *needle,
                            uint32_t wait_ms)
{
    uint32_t got = 0U;
    DWORD t0 = GetTickCount();

    if ((out == NULL) || (out_cap == 0U) || (needle == NULL)) {
        return 0U;
    }
    out[0] = '\0';
    while ((GetTickCount() - t0) < wait_ms) {
        uint8_t chunk[128];
        uint32_t n;
        uint32_t space;
        DWORD left = wait_ms - (GetTickCount() - t0);
        DWORD slice = (left > 20U) ? 20U : left;

        if (got + 1U >= out_cap) {
            break;
        }
        if (strstr(out, needle) != NULL) {
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
    if (got > 0U) {
        host_log_slave("应答", out, got);
    } else {
        (void)printf("【从机→主机】应答 [0] （等待子串超时）\n");
    }
    return got;
}

uint32_t host_wait_exact(char *out, uint32_t out_cap, const char *expect,
                         uint32_t wait_ms)
{
    uint32_t got = 0U;
    DWORD t0 = GetTickCount();

    if ((out == NULL) || (out_cap == 0U) || (expect == NULL)) {
        return 0U;
    }
    out[0] = '\0';
    while ((GetTickCount() - t0) < wait_ms) {
        uint8_t chunk[128];
        uint32_t n;
        uint32_t space;
        DWORD left = wait_ms - (GetTickCount() - t0);
        DWORD slice = (left > 20U) ? 20U : left;

        if (got + 1U >= out_cap) {
            break;
        }
        if (strcmp(out, expect) == 0) {
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
    if (got > 0U) {
        host_log_slave("应答", out, got);
    } else {
        (void)printf("【从机→主机】应答 [0] （等待精确匹配超时）\n");
    }
    return got;
}

int host_expect_quiet(uint32_t wait_ms)
{
    uint8_t chunk[64];
    DWORD t0 = GetTickCount();

    while ((GetTickCount() - t0) < wait_ms) {
        DWORD left = wait_ms - (GetTickCount() - t0);
        DWORD slice = (left > 20U) ? 20U : left;

        if (slice == 0U) {
            break;
        }
        if (link_q_read_wait(up_q, chunk, sizeof(chunk), slice) > 0U) {
            (void)printf("【从机→主机】安静窗被打断：收到非预期上行\n");
            return 0;
        }
    }
    (void)printf("【链路】安静窗 %u ms：从机无上行（符合预期）\n",
                 (unsigned)wait_ms);
    return 1;
}

uint32_t host_count_substr(const char *hay, const char *needle)
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
        (void)printf("【断言】通过：%s\n", msg);
    } else {
        check_fail++;
        (void)printf("【断言】失败：%s\n", msg);
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

void host_exit_stream_plus(void)
{
    uint32_t g = slave_rt_guard_ms();

    (void)printf("【主机】流式退出：前静默 %u ms 后发 +++\n",
                 (unsigned)(g + 40U));
    Sleep(g + 40U);
    host_send("+++");
    Sleep(g + 40U);
}

void host_exit_stream_ok(const char *tag)
{
    char rsp[HOST_COLLECT_CAP];

    host_exit_stream_plus();
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms() + 100U);
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, tag);
}

int host_enter_stream(const char *tag)
{
    char rsp[HOST_COLLECT_CAP];

    (void)printf("【主机】请求进入流式透传（%s）\n", tag);
    host_send("AT+CIPMODE=1\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    if (strcmp(rsp, HOST_RSP_OK) != 0) {
        host_check(0, tag);
        return -1;
    }
    host_send("AT+CIPSEND\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK_PROMPT, host_settle_ms());
    if (strcmp(rsp, HOST_RSP_OK_PROMPT) != 0) {
        host_check(0, tag);
        return -1;
    }
    host_check(1, tag);
    return 0;
}

int host_enter_fixed(uint32_t len, const char *tag)
{
    char rsp[HOST_COLLECT_CAP];
    char cmd[64];

    (void)printf("【主机】请求定长收数 len=%u（%s）\n", (unsigned)len, tag);
    (void)snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned)len);
    host_send(cmd);
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK_PROMPT, host_settle_ms());
    if (strcmp(rsp, HOST_RSP_OK_PROMPT) != 0) {
        host_check(0, tag);
        return -1;
    }
    host_check(1, tag);
    return 0;
}
