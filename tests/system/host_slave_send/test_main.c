/**
 * @file test_main.c
 * @brief host_slave_send 专题入口
 *
 * @details
 * Windows 双线程；仅跑 CIPMODE+CIPSEND 相关场景。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-31
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes    version
 * 2026-07-31 linzhiwei 首次发布 v1.0.0
 */
#include "host_api.h"
#include "link_q.h"
#include "scenes.h"
#include "slave_rt.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define HS_IDLE_MS 40U
#define HS_GUARD_MS 80U

link_q_t g_q_down;
link_q_t g_q_up;

int main(void)
{
    int failed_scenes = 0;
    uint32_t total = 0U;
    uint32_t fail = 0U;
    slave_rt_cfg_t cfg;

    (void)SetConsoleOutputCP(65001);
    (void)SetConsoleCP(65001);

    (void)printf("host_slave_send 专题（CIPMODE+CIPSEND）\n");
    (void)printf("说明见 TEST.md / PLAN.md\n");
    (void)printf("日志标记：【主机→从机】发送，【从机→主机】应答，【断言】检查结果\n");

    link_q_init(&g_q_down);
    link_q_init(&g_q_up);
    host_api_bind(&g_q_down, &g_q_up, HS_IDLE_MS);

    memset(&cfg, 0, sizeof(cfg));
    cfg.idle_ms = HS_IDLE_MS;
    cfg.guard_ms = HS_GUARD_MS;
    if (slave_rt_start(&g_q_down, &g_q_up, &cfg) != 0) {
        (void)printf("从机线程启动失败\n");
        return 1;
    }
    Sleep(40);

    failed_scenes += scenes_run_gate();
    failed_scenes += scenes_run_fixed();
    failed_scenes += scenes_run_stream();
    failed_scenes += scenes_run_edge();

    slave_rt_stop();
    link_q_deinit(&g_q_down);
    link_q_deinit(&g_q_up);

    host_check_stats(&total, &fail);
    (void)printf("\n==== 汇总 ====\n");
    (void)printf("断言次数: %u，失败: %u，失败场景数: %d\n",
                 (unsigned)total, (unsigned)fail, failed_scenes);
    return (fail == 0U) ? 0 : 1;
}
