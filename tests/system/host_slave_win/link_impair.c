/**
 * @file link_impair.c
 * @brief 下行链路损伤实现
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
#include "link_impair.h"

#include <windows.h>

uint32_t link_impair_write(link_q_t *q, const link_impair_t *impair,
                           const uint8_t *data, uint32_t len)
{
    link_impair_t cfg;
    uint32_t written = 0U;
    uint32_t i = 0U;
    uint32_t kept = 0U;

    if ((q == NULL) || (data == NULL) || (len == 0U)) {
        return 0U;
    }

    if (impair != NULL) {
        cfg = *impair;
    } else {
        cfg.chunk_max = 0U;
        cfg.gap_ms = 0U;
        cfg.drop_every = 0U;
        cfg.delay_ms = 0U;
    }

    if (cfg.delay_ms > 0U) {
        Sleep(cfg.delay_ms);
    }

    /* 先按丢字节规则生成要发送的流，再切块写出 */
    while (i < len) {
        uint8_t chunk[64];
        uint32_t chunk_n = 0U;
        uint32_t limit = (cfg.chunk_max == 0U) ? (uint32_t)sizeof(chunk)
                                               : cfg.chunk_max;

        if (limit > (uint32_t)sizeof(chunk)) {
            limit = (uint32_t)sizeof(chunk);
        }

        while ((i < len) && (chunk_n < limit)) {
            kept++;
            if ((cfg.drop_every > 0U) && ((kept % cfg.drop_every) == 0U)) {
                i++;
                continue;
            }
            chunk[chunk_n] = data[i];
            chunk_n++;
            i++;
        }

        if (chunk_n > 0U) {
            written += link_q_write(q, chunk, chunk_n);
            if ((cfg.gap_ms > 0U) && (i < len)) {
                Sleep(cfg.gap_ms);
            }
        }
    }
    return written;
}
