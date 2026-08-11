/**
 * @file scenes_upstream.c
 * @brief 上行方向损伤场景 U：从机→主机的链路被干扰
 *
 * @details
 * 模拟工业现场上行方向（从机→主机）的 EMI 干扰：主机收集时对上行
 * 字节流施加丢字节 + 插入噪声。验证主机按 \r\n 分帧统计仍可靠，且
 * 受损上行不会让主机把噪声误判为应答、不影响从机继续工作。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                              version
 * 2026-08-11 linzhiwei 新增上行损伤场景 U 组                v1.0.0
 */
#include "host_api.h"
#include "hs_cmd.h"
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

int scenes_run_upstream(void)
{
    char rsp[HOST_COLLECT_CAP];
    uint32_t fail0;
    int failed = 0;

    (void)printf("\n==== U upstream damage ====\n");

    /* U-01 上行插噪声：从机回多个 OK，主机收集时每 7 字节插 1 噪声 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    host_send("AT+PING\r\n");
    host_send("AT+INT=3\r\n");
    host_send("AT\r\n");
    (void)host_collect_impaired(rsp, sizeof(rsp), host_settle_ms() + 100U,
                                7U);
    /* 上行被插入 0x00 噪声：\r\n 帧边界未被破坏的应答仍可识别；
     * 因噪声可能落在 OK 中间，OK 总数 ≤ 4，且 PING 中间信息可能残缺 */
    host_check(host_count(rsp, HOST_RSP_OK) <= 4U, "U-01 OK <= 4");
    host_check(host_count(rsp, HOST_RSP_OK) >= 1U, "U-01 at least 1 OK");
    /* 噪声字节 0x00 不应被误判为任何结果（无 ERROR 凭空出现） */
    host_check(host_count(rsp, HOST_RSP_ERR) == 0U, "U-01 no fake ERROR");
    /* 从机不受上行损伤影响：随后探测 AT 应正常 */
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "U-01 recover");
    failed += scene_delta(fail0);

    /* U-02 上行丢字节：每 5 字节丢 1 个 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    host_send("AT+INT=9\r\n");
    host_send("AT\r\n");
    (void)host_collect_impaired(rsp, sizeof(rsp), host_settle_ms() + 100U,
                                5U);
    /* 丢字节会破坏部分 OK，但不应产生假 ERROR，且不崩溃 */
    host_check(host_count(rsp, HOST_RSP_ERR) == 0U, "U-02 no fake ERROR");
    host_check(host_count(rsp, HOST_RSP_OK) <= 3U, "U-02 OK <= 3");
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "U-02 recover");
    failed += scene_delta(fail0);

    /* U-03 上行被随机乱码污染：从机持续回包时上行混入高位噪声 */
    fail0 = 0U;
    host_check_stats(NULL, &fail0);
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_impaired(rsp, sizeof(rsp), host_settle_ms() + 100U,
                                3U);
    /* 即使上行重度受损（每 3 字节损伤），主机收集不应崩溃、无假 ERROR */
    host_check(host_count(rsp, HOST_RSP_ERR) == 0U, "U-03 no fake ERROR");
    host_scene_begin();
    host_send("AT\r\n");
    (void)host_collect_settle(rsp, sizeof(rsp));
    host_check(strcmp(rsp, HOST_RSP_OK) == 0, "U-03 recover");
    failed += scene_delta(fail0);

    return failed;
}
