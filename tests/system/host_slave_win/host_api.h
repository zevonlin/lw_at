/**
 * @file host_api.h
 * @brief 主机侧发送/收集/断言 API（非库核心）
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
#ifndef HOST_SLAVE_HOST_API_H
#define HOST_SLAVE_HOST_API_H

#include "link_impair.h"
#include "link_q.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_RSP_OK "\r\nOK\r\n"
#define HOST_RSP_OK_PROMPT "\r\nOK\r\n>\r\n"
#define HOST_RSP_ERR "\r\nERROR\r\n"
#define HOST_COLLECT_CAP 8192U

/**
 * @brief 绑定上下行队列（启动时调用一次）
 * @param down 下行
 * @param up   上行
 * @param idle_ms 从机 idle，用于 settle 计算
 */
void host_api_bind(link_q_t *down, link_q_t *up, uint32_t idle_ms);

/**
 * @brief 场景开始：清空队列与命令状态
 */
void host_scene_begin(void);

/**
 * @brief 明文发送（无损伤）
 * @param s 字符串
 */
void host_send(const char *s);

/**
 * @brief 二进制发送
 * @param data 数据
 * @param len  长度
 */
void host_send_bin(const uint8_t *data, uint32_t len);

/**
 * @brief 带损伤发送
 * @param s      字符串
 * @param impair 损伤；可为 NULL
 */
void host_send_impaired(const char *s, const link_impair_t *impair);

/**
 * @brief 限时收集上行
 * @param out     缓冲
 * @param out_cap 容量
 * @param wait_ms 等待
 * @return 字节数
 */
uint32_t host_collect(char *out, uint32_t out_cap, uint32_t wait_ms);

/**
 * @brief 默认 settle 时间收集
 * @param out     缓冲
 * @param out_cap 容量
 * @return 字节数
 */
uint32_t host_collect_settle(char *out, uint32_t out_cap);

/**
 * @brief 统计子串次数
 * @param hay    被搜
 * @param needle 子串
 * @return 次数
 */
uint32_t host_count(const char *hay, const char *needle);

/**
 * @brief 断言并累计
 * @param ok  非 0 通过
 * @param msg 说明
 */
void host_check(int ok, const char *msg);

/**
 * @brief 取断言计数
 * @param total 输出总数；可为 NULL
 * @param fail  输出失败；可为 NULL
 */
void host_check_stats(uint32_t *total, uint32_t *fail);

/**
 * @brief 默认 settle 毫秒
 * @return ms
 */
uint32_t host_settle_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_HOST_API_H */
