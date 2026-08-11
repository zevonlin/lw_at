/**
 * @file test_main.c
 * @brief 主机↔从机全场景测试入口
 *
 * @details
 * Windows 双线程：主线程主机，从机线程跑 lw_at；经字节队列异步交互。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 2.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                              version
 * 2026-08-11 linzhiwei 注册 U 组上行损伤场景               v2.1.0
 * 2026-07-30 linzhiwei 全场景矩阵 + hs_cmd + 损伤层         v2.0.0
 * 2026-07-30 linzhiwei 首次发布                            v1.0.0
 */
#include "host_api.h"
#include "link_q.h"
#include "scenes.h"
#include "slave_rt.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define HS_IDLE_MS 50U

link_q_t g_q_down;
link_q_t g_q_up;

int main(void)
{
    int failed_scenes = 0;
    uint32_t total = 0U;
    uint32_t fail = 0U;
    slave_rt_cfg_t cfg;

    (void)printf("host_slave_win full suite\n");
    (void)printf("see TEST.md\n");

    link_q_init(&g_q_down);
    link_q_init(&g_q_up);
    host_api_bind(&g_q_down, &g_q_up, HS_IDLE_MS);

    memset(&cfg, 0, sizeof(cfg));
    cfg.idle_ms = HS_IDLE_MS;
    if (slave_rt_start(&g_q_down, &g_q_up, &cfg) != 0) {
        (void)printf("slave_rt_start failed\n");
        return 1;
    }
    Sleep(30);

    failed_scenes += scenes_run_normal();
    failed_scenes += scenes_run_pipeline();
    failed_scenes += scenes_run_frag();
    failed_scenes += scenes_run_chars();
    failed_scenes += scenes_run_noise();
    failed_scenes += scenes_run_buffer();
    failed_scenes += scenes_run_stress();
    failed_scenes += scenes_run_transmit();
    failed_scenes += scenes_run_upstream();

    slave_rt_stop();
    link_q_deinit(&g_q_down);
    link_q_deinit(&g_q_up);

    host_check_stats(&total, &fail);
    (void)printf("\n==== 汇总 ====\n");
    (void)printf("checks: %u, failed: %u, scenes_failed: %d\n",
                 (unsigned)total, (unsigned)fail, failed_scenes);
    return (fail == 0U) ? 0 : 1;
}
