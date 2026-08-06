/**
 * @file scenes_normal.c
 * @brief 正常交互与流水线场景 N/P
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

int scenes_run_normal(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;

    (void)printf("\n==== N normal ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "N-01 AT OK");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+PING\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, "+PING:PONG") == 1U, "N-02 PING info");
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "N-02 PING OK");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+INT=42\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "N-03 INT set");
    Sleep(30);
    host_send("AT+INT?\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strstr(rsp, "+INT:42") != NULL, "N-03 INT query");
    host_check(hs_cmd_int_get() == 42, "N-03 INT state");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+STR=\"hello\"\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "N-04 STR set");
    Sleep(30);
    host_send("AT+STR?\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strstr(rsp, "+STR:hello") != NULL, "N-04 STR query");
    host_check(strcmp(hs_cmd_str_get(), "hello") == 0, "N-04 STR state");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+MIX=7,\"x\",9\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "N-05 MIX set");
    {
        int32_t a = 0;
        int32_t opt = 0;
        uint8_t present = 0U;
        char s[64];

        hs_cmd_mix_get(&a, s, sizeof(s), &opt, &present);
        host_check((a == 7) && (strcmp(s, "x") == 0) && (present == 1U) &&
                       (opt == 9),
                   "N-05 MIX state");
    }
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "N-06 first");
    host_send("AT+PING\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "N-06 second");
    failed += scene_delta(fail0);

    return failed;
}

int scenes_run_pipeline(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    int i;

    (void)printf("\n==== P pipeline ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    host_send("AT\r\n");
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 50U);
    host_check(host_count(rsp, HOST_RSP_OK) == 2U, "P-01 two OK");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    hs_cmd_slow_set_ms(40U);
    host_send("AT+SLOW\r\n");
    host_send("AT\r\n");
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 120U);
    host_check(host_count(rsp, HOST_RSP_OK) == 2U, "P-02 SLOW then AT");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    for (i = 0; i < 32; i++) {
        host_send("AT\r\n");
    }
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 400U);
    host_check(host_count(rsp, HOST_RSP_OK) == 32U, "P-02 flood 32");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    host_send("AT\r\n");
    Sleep(10);
    host_send("AT+INT=3\r\n");
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 100U);
    host_check(host_count(rsp, HOST_RSP_OK) == 3U, "P-03 pipeline 3");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\nAT+PING\r\nAT\r\n");
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 100U);
    host_check(host_count(rsp, HOST_RSP_OK) == 3U, "P-04 sticky 3");
    host_check(host_count(rsp, "+PING:PONG") == 1U, "P-04 sticky ping");
    failed += scene_delta(fail0);

    return failed;
}
