/**
 * @file link_impair.h
 * @brief 下行链路损伤：切块、丢字节、延迟（非库核心）
 *
 * @details
 * 主机经本层写入下行队列，模拟 UART 分片与 RX 丢数。
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
#ifndef HOST_SLAVE_LINK_IMPAIR_H
#define HOST_SLAVE_LINK_IMPAIR_H

#include "link_q.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 下行损伤参数；全 0 表示直接整段写入
 */
typedef struct {
    uint32_t chunk_max;   /**< 每片最大字节；0 表示不切块 */
    uint32_t gap_ms;      /**< 片间 Sleep 毫秒 */
    uint32_t drop_every;  /**< 每 N 字节丢 1；0 表示不丢 */
    uint32_t delay_ms;    /**< 发送前延迟 */
} link_impair_t;

/**
 * @brief 按损伤参数写入下行队列
 * @param q       下行队列
 * @param impair  损伤参数；可为 NULL（等同全 0）
 * @param data    数据
 * @param len     长度
 * @return 实际写入字节数（丢弃的不计）
 */
uint32_t link_impair_write(link_q_t *q, const link_impair_t *impair,
                           const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_LINK_IMPAIR_H */
