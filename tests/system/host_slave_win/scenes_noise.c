/**
 * @file scenes_noise.c
 * @brief 乱码/干扰/异常命令 G/X
 *
 * @details
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-30
 * @version 1.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes    version
 * 2026-08-11 linzhiwei 新增 G-06 乱码刷屏 / G-07 stuck-bit 恢复 v1.1.0
 * 2026-07-30 linzhiwei 首次发布 v1.0.0
 */
#include "host_api.h"
#include "link_impair.h"
#include "link_q.h"
#include "scenes.h"
#include "slave_rt.h"

#include <stdio.h>
#include <string.h>

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

int scenes_run_noise(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    static const uint8_t junk[] = { 0x00, 0xFF, (uint8_t)'Z' };
    static const uint8_t mid[] = { 0x01, 0x02, 0x7F };
    static const uint8_t hi[] = { 0x80, 0xFE, 0xFF };
    link_impair_t impair;

    (void)printf("\n==== G/X noise exception ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send_bin(junk, (uint32_t)sizeof(junk));
    host_send("\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "G-01 noise ERROR");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-01 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    host_send_bin(mid, (uint32_t)sizeof(mid));
    host_send("\r\nAT\r\n");
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 80U);
    host_check(host_count(rsp, HOST_RSP_OK) == 2U, "G-02 two OK");
    host_check(host_count(rsp, HOST_RSP_ERR) == 1U, "G-02 one ERROR");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    memset(&impair, 0, sizeof(impair));
    impair.drop_every = 4U;
    host_send_impaired("AT+PING\r\n", &impair);
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U ||
                   host_count(rsp, HOST_RSP_OK) == 0U,
               "G-03 drop damaged");
    {
        slave_rt_cfg_t cfg;

        memset(&cfg, 0, sizeof(cfg));
        cfg.idle_ms = 50U;
        link_q_reset(&g_q_down);
        link_q_reset(&g_q_up);
        (void)slave_rt_reinit(&cfg);
        Sleep(30);
    }
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-03 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U, "G-04 lost LF ERROR");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-04 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send_bin(hi, (uint32_t)sizeof(hi));
    host_send("\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "G-05 hi bytes");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-05 recover");
    failed += scene_delta(fail0);

    /* G-06 大规模随机乱码刷屏：300+ 字节全范围乱码连续灌入 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    {
        uint32_t seed = 0x9E3779B9U;
        uint32_t i;
        uint8_t noise[320];
        int err_count = 0;

        for (i = 0U; i < sizeof(noise); i++) {
            seed = (1664525U * seed) + 1013904223U;
            noise[i] = (uint8_t)(seed >> 24U);
            /* 每 64 字节强制插入 \r\n，制造可解析的乱码行 */
            if ((i % 64U) == 63U) {
                noise[i] = (uint8_t)'\n';
                noise[i - 1U] = (uint8_t)'\r';
            }
        }
        /* 分段灌入，从机实时消化（模拟刷屏后主机停止发送） */
        for (i = 0U; i < sizeof(noise); i += 64U) {
            host_send_bin(&noise[i], 64U);
            Sleep(host_settle_ms() + 30U);
        }
        (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 100U);
        /* 乱码行应被解析为 ERROR（含 \r\n 的），或溢出丢弃；允许混合 */
        err_count = (int)host_count(rsp, HOST_RSP_ERR);
        (void)err_count;
        host_send("AT\r\n");
        (void)host_collect_settle(rsp, sizeof(rsp));
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-06 recover AT");
    }
    failed += scene_delta(fail0);

    /* G-07 stuck-bit 重复字节：RX 线故障连续 0xFF / 0x00 灌入 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    {
        uint8_t ones[200];
        uint8_t zeros[200];

        memset(ones, 0xFF, sizeof(ones));
        memset(zeros, 0x00, sizeof(zeros));
        host_send_bin(ones, (uint32_t)sizeof(ones));
        (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 80U);
        host_send("AT\r\n");
        (void)host_collect_settle(rsp, sizeof(rsp));
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-07 ones recover");
        host_send_bin(zeros, (uint32_t)sizeof(zeros));
        (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 80U);
        host_send("AT\r\n");
        (void)host_collect_settle(rsp, sizeof(rsp));
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "G-07 zeros recover");
    }
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+NOPE\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "X-01 NOPE");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+FAIL\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "X-02 FAIL");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+PING=?\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "X-03 PING test missing");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+FAIL\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_send("AT+NOPE\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "X-04 recover after errors");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("PING\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "X-05 no AT prefix");
    failed += scene_delta(fail0);

    return failed;
}
