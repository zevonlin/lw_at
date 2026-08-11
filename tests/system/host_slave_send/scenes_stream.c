/**
 * @file scenes_stream.c
 * @brief 流式无参 CIPSEND 场景
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
 * Date       Author    Notes                       version
 * 2026-08-11 linzhiwei 流式退出改 host_exit_stream_ok v1.2.0
 * 2026-07-31 linzhiwei enter 门禁；安静窗验无结果码 v1.1.0
 * 2026-07-31 linzhiwei 首次发布                     v1.0.0
 */
#include "host_api.h"
#include "scenes.h"
#include "slave_rt.h"

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

int scenes_run_stream(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    uint32_t guard;

    (void)printf("\n==== 流式组 S：无参 CIPSEND ====\n");
    guard = slave_rt_guard_ms();

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("S-01 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("he");
        Sleep(5);
        host_send("llo");
        Sleep(60);
        host_check(strcmp(slave_rt_sink_get(), "hello") == 0, "S-01 sink");
        host_exit_stream_ok("S-01 exit OK");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "S-01 after AT");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("S-02 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("d");
        Sleep(20);
        host_send("+++");
        Sleep(60);
        host_check(strcmp(slave_rt_sink_get(), "d+++") == 0, "S-02 false +++");
        host_exit_stream_ok("S-02 exit OK");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "S-02 recover");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("S-03 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("AT\r\n");
        Sleep(60);
        host_check(strcmp(slave_rt_sink_get(), "AT\r\n") == 0, "S-03 AT as data");
        host_exit_stream_ok("S-03 exit OK");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "S-03 cmd again");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("S-04 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        char bulk[201];

        memset(bulk, 'B', 200);
        bulk[200] = '\0';
        host_send(bulk);
        Sleep(100);
        host_check(slave_rt_sink_len() == 200U, "S-04 len 200");
        host_check(strcmp(slave_rt_sink_get(), bulk) == 0, "S-04 content");
        host_exit_stream_ok("S-04 exit OK");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "S-04 back");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("S-05 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        Sleep(guard + 40U);
        host_send("+++");
        Sleep(20);
        host_send("z");
        Sleep(80);
        host_check(strcmp(slave_rt_sink_get(), "+++z") == 0, "S-05 restore +++");
        host_exit_stream_ok("S-05 exit OK");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "S-05 recover");
        failed += scene_delta(fail0);
    }

    return failed;
}
