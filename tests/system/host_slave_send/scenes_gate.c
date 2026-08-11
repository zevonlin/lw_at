/**
 * @file scenes_gate.c
 * @brief 门控场景：CIPMODE / 非法 CIPSEND
 *
 * @details
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 1.2.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                    version
 * 2026-08-11 linzhiwei 流式退出改 host_exit_stream_ok v1.2.0
 * 2026-07-31 linzhiwei 精确应答；enter 失败即止  v1.1.0
 * 2026-07-31 linzhiwei 首次发布                  v1.0.0
 */
#include "host_api.h"
#include "scenes.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

/**
 * @brief 相对失败增量
 * @param fail0 进入前失败数
 * @return 0/1
 */
static int scene_delta(uint32_t fail0)
{
    uint32_t fail = 0U;

    host_check_stats(NULL, &fail);
    return (fail > fail0) ? 1 : 0;
}

int scenes_run_gate(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;

    (void)printf("\n==== 门控组 G：CIPMODE / 非法 CIPSEND ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPMODE=0\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-01 CIPMODE=0");
    host_send("AT+CIPSEND\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "G-01 CIPSEND without mode");
    host_send("AT\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-01 still command");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("G-02 enter stream") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_exit_stream_ok("G-02 exit OK");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-02 exit AT");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPMODE=2\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "G-03 CIPMODE=2");
    host_send("AT+CIPMODE=1\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-03 CIPMODE=1");
    host_send("AT+CIPMODE?\r\n");
    (void)host_wait_contains(rsp, sizeof(rsp), "+CIPMODE:1", host_settle_ms());
    host_check(strcmp(rsp, "\r\n+CIPMODE:1\r\n\r\nOK\r\n") == 0, "G-05 query exact");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPSEND=0\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "G-04 CIPSEND=0");
    host_send("AT+CIPSEND=abc\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "G-04 CIPSEND=abc");
    host_send("AT+CIPSEND=-1\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "G-04 CIPSEND=-1");
    failed += scene_delta(fail0);

    return failed;
}
