/**
 * @file scenes_edge.c
 * @brief 异步粘连、切换、打断、缓冲与真实交互加固场景
 *
 * @details
 * A/X/I/B/R 组。流式溢出：内核在 DATA 下清除 overflow 标志且不回 ERROR
 * （见 core_data_process_stream）；B-01 按该契约验丢弃与可退出恢复。
 * 命令模式溢出另见 B-CMD。
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
 * Date       Author    Notes                              version
 * 2026-07-31 linzhiwei 加严断言；补 R/B-CMD；对齐溢出契约 v1.1.0
 * 2026-07-31 linzhiwei 首次发布                            v1.0.0
 */
#include "host_api.h"
#include "scenes.h"
#include "send_cmd.h"
#include "slave_rt.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

#define A03_EXPECT "\r\nOK\r\n\r\nOK\r\n>\r\n"

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

int scenes_run_edge(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    slave_rt_cfg_t small_cfg;
    slave_rt_cfg_t normal_cfg;

    (void)printf("\n==== 边界组 A/X/I/B/R：异步/切换/打断/缓冲/加固 ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPMODE=1\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "A-01 CIPMODE");
    host_send("AT+CIPSEND\r\nhello");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK_PROMPT, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK_PROMPT) == 0, "A-01 prompt");
    Sleep(80);
    host_check(slave_rt_sink_len() == 0U, "A-01 glued payload dropped");
    host_send("hello");
    Sleep(80);
    host_check(strcmp(slave_rt_sink_get(), "hello") == 0, "A-01 resend ok");
    host_exit_stream_plus();
    host_send("AT\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "A-01 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPSEND=4\r\n1234");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK_PROMPT, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK_PROMPT) == 0, "A-02 prompt");
    Sleep(80);
    host_check(send_cmd_fixed_len_get() == 0U, "A-02 glued dropped");
    host_check(send_cmd_fixed_done_got() == 0U, "A-02 not done yet");
    host_send("1234");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "A-02 after resend");
    host_check(strcmp(send_cmd_fixed_buf_get(), "1234") == 0, "A-02 payload");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPMODE=1\r\nAT+CIPSEND\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), A03_EXPECT, host_settle_ms() + 80U);
    host_check(strcmp(rsp, A03_EXPECT) == 0, "A-03 exact OK+PROMPT");
    host_exit_stream_plus();
    host_send("AT\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "A-03 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("A-04 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("NOW");
        Sleep(60);
        host_check(strcmp(slave_rt_sink_get(), "NOW") == 0, "A-04 immediate data");
        host_exit_stream_plus();
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(2U, "X-01 fixed1") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("ab");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "X-01 fixed1 OK");
        if (host_enter_stream("X-01 stream") != 0) {
            failed += scene_delta(fail0);
        } else {
            host_send("xy");
            Sleep(60);
            host_check(strcmp(slave_rt_sink_get(), "xy") == 0, "X-01 stream data");
            host_exit_stream_plus();
            if (host_enter_fixed(2U, "X-01 fixed2") != 0) {
                failed += scene_delta(fail0);
            } else {
                host_send("cd");
                (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK,
                                      host_settle_ms());
                host_check(strcmp(rsp, HOST_RSP_OK) == 0, "X-01 fixed2 OK");
                failed += scene_delta(fail0);
            }
        }
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("X-02 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_exit_stream_plus();
        host_send("AT+CIPMODE=0\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "X-02 mode0");
        host_send("AT+CIPSEND\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "X-02 send fail");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("X-03 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("AT+CIPSEND\r\n");
        Sleep(60);
        host_check(strcmp(slave_rt_sink_get(), "AT+CIPSEND\r\n") == 0,
                   "X-03 as data");
        host_exit_stream_plus();
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "X-03 recover");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(10U, "I-01 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("ABC");
        Sleep(40);
        slave_rt_request_data_exit();
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR,
                              host_settle_ms() + 100U);
        host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "I-01 ERROR");
        host_check(send_cmd_fixed_done_got() == 3U, "I-01 got=3");
        host_check(strcmp(send_cmd_fixed_buf_get(), "ABC") == 0, "I-01 partial");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "I-01 recover");
        failed += scene_delta(fail0);
    }

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("I-02 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("zz");
        Sleep(40);
        slave_rt_request_data_exit();
        host_check(host_expect_quiet(100U) != 0, "I-02 quiet after app exit");
        host_check(strcmp(slave_rt_sink_get(), "zz") == 0, "I-02 sink kept");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "I-02 recover");
        failed += scene_delta(fail0);
    }

    /* I-03：假 +++ 后，再用合法 +++ 取消（CIPABORT=1） */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIPABORT=1\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    if (host_enter_fixed(20U, "I-03 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("x+++");
        Sleep(80);
        host_check(send_cmd_fixed_done_got() == 0U, "I-03 not aborted yet");
        host_check(strcmp(send_cmd_fixed_buf_get(), "x+++") == 0, "I-03 as data");
        host_exit_stream_plus();
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR,
                              host_settle_ms() + 100U);
        host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "I-03 legal +++ ERROR");
        host_check(send_cmd_fixed_done_got() == 4U, "I-03 got=4");
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "I-03 recover");
        failed += scene_delta(fail0);
    }

    /*
     * B-01：小 rx + 流式灌入。
     * 契约：DATA 下 overflow 被吞掉不回 ERROR；应观察到丢字节，且仍能 +++ 退出。
     */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    memset(&small_cfg, 0, sizeof(small_cfg));
    small_cfg.idle_ms = 40U;
    small_cfg.guard_ms = 80U;
    small_cfg.rx_buf_size = 32U;
    small_cfg.line_buf_size = 64U;
    host_check(slave_rt_reinit(&small_cfg) == 0, "B-01 reinit small");
    Sleep(40);
    if (host_enter_stream("B-01 enter") != 0) {
        failed += scene_delta(fail0);
    } else {
        char flood[96];
        uint32_t sent = (uint32_t)(sizeof(flood) - 1U);

        memset(flood, 'F', sent);
        flood[sent] = '\0';
        host_send(flood);
        host_send(flood);
        Sleep(150);
        host_check(slave_rt_sink_len() < (sent * 2U), "B-01 dropped some");
        host_check(slave_rt_sink_len() > 0U, "B-01 got some");
        host_check(host_expect_quiet(60U) != 0, "B-01 no ERROR on stream ovf");
        host_exit_stream_plus();
        host_send("AT\r\n");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK,
                              host_settle_ms() + 100U);
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "B-01 recover AT");
        failed += scene_delta(fail0);
    }

    /* B-CMD：命令模式溢出 → 精确 ERROR，再 AT 恢复 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    memset(&small_cfg, 0, sizeof(small_cfg));
    small_cfg.idle_ms = 40U;
    small_cfg.guard_ms = 80U;
    small_cfg.rx_buf_size = 32U;
    small_cfg.line_buf_size = 64U;
    host_check(slave_rt_reinit(&small_cfg) == 0, "B-CMD reinit");
    Sleep(40);
    {
        char junk[64];

        memset(junk, 'Z', sizeof(junk) - 1U);
        junk[sizeof(junk) - 1U] = '\0';
        host_send(junk);
        host_send(junk);
    }
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_ERR, host_settle_ms() + 80U);
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "B-CMD overflow ERROR");
    host_send("AT\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "B-CMD recover AT");
    failed += scene_delta(fail0);

    /* B-02：恢复默认缓冲，状态不串扰 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    memset(&normal_cfg, 0, sizeof(normal_cfg));
    normal_cfg.idle_ms = 40U;
    normal_cfg.guard_ms = 80U;
    host_check(slave_rt_reinit(&normal_cfg) == 0, "B-02 reinit normal");
    Sleep(40);
    host_scene_begin();
    host_check(send_cmd_cipmode_get() == 0, "B-02 cipmode reset");
    host_send("AT\r\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "B-02 clean AT");
    failed += scene_delta(fail0);

    /* R-01：+++ 退出后几乎立刻再进流式 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_stream("R-01 first") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("a");
        Sleep(30);
        host_exit_stream_plus();
        slave_rt_sink_clear();
        if (host_enter_stream("R-01 reenter") != 0) {
            failed += scene_delta(fail0);
        } else {
            host_send("b");
            Sleep(60);
            host_check(strcmp(slave_rt_sink_get(), "b") == 0, "R-01 sink only b");
            host_exit_stream_plus();
            host_send("AT\r\n");
            (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
            host_check(strcmp(rsp, HOST_RSP_OK) == 0, "R-01 recover");
            failed += scene_delta(fail0);
        }
    }

    /* R-02：定长载荷含 0x00 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(5U, "R-02 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        uint8_t bin[5] = {0x41U, 0x00U, 0x42U, 0x00U, 0x43U};

        host_send_bin(bin, 5U);
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "R-02 OK");
        host_check(send_cmd_fixed_done_got() == 5U, "R-02 got=5");
        host_check(send_cmd_fixed_len_get() == 5U, "R-02 len=5");
        host_check(memcmp(send_cmd_fixed_buf_get(), bin, 5U) == 0, "R-02 bytes");
        failed += scene_delta(fail0);
    }

    /* R-03：提示后无 Sleep 立即发定长（主机流水线） */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    if (host_enter_fixed(4U, "R-03 prompt") != 0) {
        failed += scene_delta(fail0);
    } else {
        host_send("WXYZ");
        (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
        host_check(strcmp(rsp, HOST_RSP_OK) == 0, "R-03 OK");
        host_check(strcmp(send_cmd_fixed_buf_get(), "WXYZ") == 0, "R-03 payload");
        failed += scene_delta(fail0);
    }

    /* R-04：命令按字节切分后再定长 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+CIP");
    Sleep(5);
    host_send("SEND=3\r");
    Sleep(5);
    host_send("\n");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK_PROMPT, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK_PROMPT) == 0, "R-04 frag prompt");
    host_send("1");
    Sleep(5);
    host_send("2");
    Sleep(5);
    host_send("3");
    (void)host_wait_exact(rsp, sizeof(rsp), HOST_RSP_OK, host_settle_ms());
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "R-04 OK");
    host_check(strcmp(send_cmd_fixed_buf_get(), "123") == 0, "R-04 payload");
    failed += scene_delta(fail0);

    return failed;
}
