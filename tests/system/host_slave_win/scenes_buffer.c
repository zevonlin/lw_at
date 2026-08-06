/**
 * @file scenes_buffer.c
 * @brief 缓存与主机接收 B/R
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
#include "link_q.h"
#include "scenes.h"
#include "slave_rt.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

extern link_q_t g_q_down;
extern link_q_t g_q_up;

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

int scenes_run_buffer(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    slave_rt_cfg_t small_cfg;
    slave_rt_cfg_t normal_cfg;
    uint8_t flood[64];

    (void)printf("\n==== B/R buffer receive ====\n");

    memset(&small_cfg, 0, sizeof(small_cfg));
    small_cfg.idle_ms = 50U;
    small_cfg.rx_buf_size = 16U;
    small_cfg.line_buf_size = 64U;
    memset(&normal_cfg, 0, sizeof(normal_cfg));
    normal_cfg.idle_ms = 50U;

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_check(slave_rt_reinit(&small_cfg) == 0, "B reinit small");
    Sleep(30);
    host_scene_begin();
    memset(flood, (int)'X', sizeof(flood));
    host_send_bin(flood, (uint32_t)sizeof(flood));
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "B-01 overflow ERROR");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "B-01 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "B-02 short OK on small rx");
    failed += scene_delta(fail0);

    (void)slave_rt_reinit(&normal_cfg);
    Sleep(30);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    {
        uint8_t big[LINK_Q_SIZE + 128U];

        /* 单次持锁写入超过容量，从机来不及排空即可观察到 drop */
        memset(big, (int)'Z', sizeof(big));
        (void)link_q_write(&g_q_down, big, (uint32_t)sizeof(big));
        host_check(link_q_drop_count(&g_q_down) > 0U, "B-04 link_q drop observed");
    }
    link_q_reset(&g_q_down);
    link_q_reset(&g_q_up);
    (void)slave_rt_reinit(&normal_cfg);
    Sleep(30);
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "B-04 probe OK");
    failed += scene_delta(fail0);

    /* R-01 慢读 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\nAT\r\nAT\r\n");
    Sleep(host_settle_ms() + 50U);
    (void)host_collect(rsp, sizeof(rsp), 200U);
    host_check(host_count(rsp, HOST_RSP_OK) == 3U, "R-01 late read 3 OK");
    failed += scene_delta(fail0);

    /* R-03 超时空 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    (void)host_collect(rsp, sizeof(rsp), 80U);
    host_check(strcmp(rsp, "") == 0, "R-03 timeout empty");
    failed += scene_delta(fail0);

    return failed;
}
