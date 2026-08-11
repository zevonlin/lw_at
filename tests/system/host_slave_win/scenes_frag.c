/**
 * @file scenes_frag.c
 * @brief 分片/粘包/半包/行尾/字符场景 I/K/F/C
 *
 * @details
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 1.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes    version
 * 2026-08-11 linzhiwei I-01/I-03/K-02/F-01 半行空闲超时作废断言 v1.1.0
 * 2026-07-30 linzhiwei 首次发布 v1.0.0
 */
#include "host_api.h"
#include "hs_cmd.h"
#include "link_impair.h"
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

int scenes_run_frag(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    link_impair_t impair;

    (void)printf("\n==== I/K frag sticky framing ====\n");

    /* I-01 半行停顿 > idle 作废 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+STR=\"he");
    Sleep(host_settle_ms());
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U, "I-01 half ERROR");
    host_send("llo\"\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U, "I-01 tail ERROR");
    failed += scene_delta(fail0);

    /* I-02 逐字节 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    memset(&impair, 0, sizeof(impair));
    impair.chunk_max = 1U;
    impair.gap_ms = 1U;
    host_send_impaired("AT+INT=5\r\n", &impair);
    (void)host_collect(rsp, sizeof(rsp), host_settle_ms() + 80U);
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "I-02 byte OK");
    host_check(hs_cmd_int_get() == 5, "I-02 int 5");
    failed += scene_delta(fail0);

    /* I-03 / F-07 CR LF split：CR 半行超时作废；孤立 LF 亦不成行 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r");
    Sleep(host_settle_ms());
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "I-03 only CR ERROR");
    host_send("\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "I-03 lone LF ERROR");
    failed += scene_delta(fail0);

    /* K-02 完整+半行：完整行 OK，半行超时作废 ERROR */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\nAT+INT=");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "K-02 first OK");
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U, "K-02 half ERROR");
    host_send("8\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U, "K-02 tail ERROR");
    failed += scene_delta(fail0);

    /* K-03 半包放弃 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+IN");
    Sleep(20);
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U, "K-03 dirty ERROR");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "K-03 recover");
    failed += scene_delta(fail0);

    /* F-01 仅 CR，再补 LF：CR 半行超时作废；孤立 LF 不成行 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r");
    Sleep(host_settle_ms());
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "F-01 only CR ERROR");
    host_send("\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "F-01 lone LF ERROR");
    failed += scene_delta(fail0);

    /* F-02 仅 LF */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 0U, "F-02 LF only no OK");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    /* 可能拼脏 ERROR 或 OK；允许 ERROR 后再探活 */
    if (host_count(rsp, HOST_RSP_OK) == 0U) {
        host_send("AT\r\n");
        (void)host_collect_settle(rsp, sizeof(rsp));
    }
    host_check(host_count(rsp, HOST_RSP_OK) >= 1U, "F-02 recover OK");
    failed += scene_delta(fail0);

    /* F-05 双 CRLF */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "F-05 one OK");
    failed += scene_delta(fail0);

    /* F-06 空行夹两命令 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n\r\nAT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 2U, "F-06 two OK");
    failed += scene_delta(fail0);

    /* F-08 仅 CR 后新帧 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r");
    Sleep(20);
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_ERR) >= 1U, "F-08 dirty ERROR");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "F-08 recover");
    failed += scene_delta(fail0);

    return failed;
}

int scenes_run_chars(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;
    char long_cmd[HS_CMD_STR_CAP + 32];
    size_t i;

    (void)printf("\n==== C chars ====\n");

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("at\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "C-01 lowercase");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, "") == 0, "C-02 empty line");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+STR=\"a,b\"\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "C-03 comma str OK");
    host_check(strcmp(hs_cmd_str_get(), "a,b") == 0, "C-03 comma value");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+STR=\"4");
    Sleep(5);
    host_send("2\"\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(host_count(rsp, HOST_RSP_OK) == 1U, "C-04 quote frag");
    host_check(strcmp(hs_cmd_str_get(), "42") == 0, "C-04 value");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+STR=\"12\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "C-05 unclosed");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "C-05 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    (void)snprintf(long_cmd, sizeof(long_cmd), "AT+STR=\"");
    for (i = 0; i < HS_CMD_STR_CAP; i++) {
        size_t n = strlen(long_cmd);
        if (n + 2U < sizeof(long_cmd)) {
            long_cmd[n] = 'X';
            long_cmd[n + 1U] = '\0';
        }
    }
    (void)strncat(long_cmd, "\"\r\n", sizeof(long_cmd) - strlen(long_cmd) - 1U);
    host_send(long_cmd);
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "C-07 overlong STR");
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "C-07 recover");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT+INT=abc\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "C-08 INT abc");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send(" AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "C-09 leading space");
    failed += scene_delta(fail0);

    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT;AT+PING\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_ERR) == 0, "C-10 multi cmd line");
    failed += scene_delta(fail0);

    return failed;
}
