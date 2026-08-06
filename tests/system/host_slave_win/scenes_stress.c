/**
 * @file scenes_stress.c
 * @brief 压力场景 S
 *
 * @details
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
#include "host_api.h"
#include "hs_cmd.h"
#include "link_impair.h"
#include "scenes.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

/**
 * @brief 场景失败增量
 * @param fail0 进入前失败数
 * @return 0/1
 */
static int scene_delta(uint32_t fail0)
{
    uint32_t fail = 0U;

    host_check_stats(NULL, &fail);
    return (fail > fail0) ? 1 : 0;
}

int scenes_run_stress(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    int i;
    link_impair_t impair;
    DWORD t0;
    int sent = 0;

    (void)printf("\n==== S stress ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    for (i = 0; i < 100; i++) {
        host_send("AT\r\n");
    }
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 800U);
    host_check(host_count(rsp, HOST_RSP_OK) == 100U, "S-01 100 OK");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    for (i = 0; i < 20; i++) {
        char cmd[32];

        (void)snprintf(cmd, sizeof(cmd), "AT+INT=%d\r\n", i);
        host_send(cmd);
        host_send("AT+STR=\"s\"\r\n");
    }
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 500U);
    host_check(host_count(rsp, HOST_RSP_OK) == 40U, "S-02 alt 40 OK");
    host_check(hs_cmd_int_get() == 19, "S-02 last INT");
    host_check(strcmp(hs_cmd_str_get(), "s") == 0, "S-02 last STR");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    t0 = GetTickCount();
    while ((GetTickCount() - t0) < 1000U) {
        host_send("AT\r\n");
        sent++;
        if (sent >= 80) {
            break;
        }
    }
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 1500U);
    host_check(host_count(rsp, HOST_RSP_OK) == (uint32_t)sent, "S-03 burst OK");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "S-03 probe");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    memset(&impair, 0, sizeof(impair));
    impair.chunk_max = 1U;
    for (i = 0; i < 20; i++) {
        host_send_impaired("AT+PING\r\n", &impair);
    }
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 600U);
    host_check(host_count(rsp, HOST_RSP_OK) == 20U, "S-04 chunk1 20");
    host_check(host_count(rsp, "+PING:PONG") == 20U, "S-04 ping info");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    hs_cmd_slow_set_ms(25U);
    host_send("AT+SLOW\r\n");
    host_send("AT\r\n");
    host_send("AT+SLOW\r\n");
    host_send("AT\r\n");
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 200U);
    host_check(host_count(rsp, HOST_RSP_OK) == 4U, "S-05 slow mix");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    {
        char blob[16 * 4 + 1];
        int n = 0;

        blob[0] = '\0';
        for (i = 0; i < 16; i++) {
            (void)strcat(blob, "AT\r\n");
            n++;
        }
        host_send(blob);
        (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 200U);
        host_check(host_count(rsp, HOST_RSP_OK) == (uint32_t)n, "S-06 sticky 16");
    }
    failed += scene_delta(fail0);

    return failed;
}
