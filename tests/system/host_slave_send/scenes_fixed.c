/**
 * @file scenes_fixed.c
 * @brief 定长 CIPSEND=<len> 场景
 *
 * @details
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-31
 * @version 1.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                         version
 * 2026-07-31 linzhiwei 精确 OK；enter 门禁；独立 F-06 v1.1.0
 * 2026-07-31 linzhiwei 首次发布                       v1.0.0
 */
#include "host_api.h"
#include "scenes.h"
#include "send_cmd.h"

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

int scenes_run_fixed(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;

    (void)printf("\n==== 定长组 F：CIPSEND=<len> ====\n");

    /* F-06：规范主机 — 等完整提示后再发 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(5U, "F-06 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("ABCDE");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-06 full OK");
        host_check(strcmp(send_cmd_fixed_buf_get(), "ABCDE") == 0, "F-06 payload");
        host_check(send_cmd_fixed_done_got() == 5U, "F-06 got=5");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-06 after AT");
        failed += scene_delta(fail0);
    }

    /* F-01：与 F-06 同路径，保留整包名以便对照 PLAN */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(5U, "F-01 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("ABCDE");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-01 full OK");
        host_check(strcmp(send_cmd_fixed_buf_get(), "ABCDE") == 0, "F-01 payload");
        host_check(send_cmd_fixed_done_got() == 5U, "F-01 got=5");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(5U, "F-02 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("AB");
        Sleep(20);
        host_send("CDE");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-02 OK");
        host_check(strcmp(send_cmd_fixed_buf_get(), "ABCDE") == 0, "F-02 payload");
        host_check(send_cmd_fixed_done_got() == 5U, "F-02 got=5");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(3U, "F-03 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("XYZAT\r\n");
        (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 120U);
        host_check(strcmp(send_cmd_fixed_buf_get(), "XYZ") == 0, "F-03 fixed XYZ");
        host_check(send_cmd_fixed_done_got() == 3U, "F-03 got=3");
        host_check(strcmp(rsp, "\r\nOK\r\n\r\nOK\r\n") == 0, "F-03 exact two OK");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(4U, "F-04 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-04 filled by ATCRLF");
        host_check(strcmp(send_cmd_fixed_buf_get(), "AT\r\n") == 0, "F-04 as data");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPABORT=1\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-05 CIPABORT=1");
    if (host_enter_fixed(20U, "F-05 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("HI");
        Sleep(40);
        host_exit_stream_plus();
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR,
                              host_settle_ms() + 100U);
        host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "F-05 abort ERROR");
        host_check(send_cmd_fixed_done_got() == 2U, "F-05 got=2");
        host_check(strcmp(send_cmd_fixed_buf_get(), "HI") == 0, "F-05 partial");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-05 recover AT");
        failed += scene_delta(fail0);
    }

    return failed;
}
