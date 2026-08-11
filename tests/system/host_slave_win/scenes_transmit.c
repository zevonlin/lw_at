/**
 * @file scenes_transmit.c
 * @brief 透传双端场景 T
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
 * Date       Author    Notes    version
 * 2026-08-11 linzhiwei 退出后消费 OK 回包再发命令 v1.2.0
 * 2026-07-31 linzhiwei 进入改为 CIPMODE+CIPSEND v1.1.0
 * 2026-07-30 linzhiwei 首次发布 v1.0.0
 */
#include "host_api.h"
#include "scenes.h"
#include "slave_rt.h"

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

/**
 * @brief 合法 +++ 退出：前静默、发 +++、后静默
 */
static void host_exit_transmit(void)
{
    Sleep(120);
    host_send("+++");
    Sleep(120);
}

/**
 * @brief 按 ESP 用法进入流式透传
 * @param tag 断言标签前缀
 */
static void host_enter_transmit(const char *tag)
{
    char rsp[HOST_COLLECT_CAP];

    host_send("AT+CIPMODE=1\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, tag);
    host_send("AT+CIPSEND\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK_PROMPT) == 0, tag);
}

int scenes_run_transmit(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;

    (void)printf("\n==== T transmit ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    slave_rt_sink_clear();
    host_enter_transmit("T-01 enter");
    host_send("he");
    Sleep(5);
    host_send("llo");
    Sleep(80);
    host_check(strcmp(slave_rt_sink_get(), "hello") == 0, "T-01 sink hello");
    host_exit_transmit();
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-01 exit OK");
    Sleep(80);
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-01 after exit AT");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    slave_rt_sink_clear();
    host_enter_transmit("T-02 enter");
    host_send("d");
    Sleep(20);
    /* 前静默不足：整段 +++ 作数据 */
    host_send("+++");
    Sleep(80);
    host_check(strcmp(slave_rt_sink_get(), "d+++") == 0, "T-02 false +++");
    host_exit_transmit();
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-02 exit OK");
    Sleep(80);
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-02 recover AT");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    slave_rt_sink_clear();
    host_enter_transmit("T-03 enter");
    {
        char bulk[101];

        memset(bulk, 'A', 100);
        bulk[100] = '\0';
        host_send(bulk);
    }
    Sleep(100);
    host_check(slave_rt_sink_len() == 100U, "T-03 sink 100");
    host_exit_transmit();
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-03 exit OK");
    Sleep(80);
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-03 back to cmd");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    slave_rt_sink_clear();
    host_enter_transmit("T-04 enter");
    host_send("AT\r\n");
    Sleep(80);
    host_check(strcmp(slave_rt_sink_get(), "AT\r\n") == 0, "T-04 AT as data");
    host_exit_transmit();
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-04 exit OK");
    Sleep(80);
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "T-04 cmd again");
    failed += scene_delta(fail0);

    return failed;
}
