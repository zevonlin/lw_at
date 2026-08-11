/**
 * @file host_api.h
 * @brief host_slave_send 主机侧发送/收集/断言
 *
 * @details
 * 提供精确应答等待与安静窗检查，避免「见子串即过」的松断言。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 1.3.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                              version
 * 2026-08-11 linzhiwei 新增 host_exit_stream_ok 消费退出回包 v1.3.0
 * 2026-07-31 linzhiwei 中文双端日志与控制台 UTF-8         v1.2.0
 * 2026-07-31 linzhiwei 精确等待/安静窗/子串计数           v1.1.0
 * 2026-07-31 linzhiwei 首次发布                            v1.0.0
 */
#ifndef HOST_SLAVE_SEND_HOST_API_H
#define HOST_SLAVE_SEND_HOST_API_H

#include "link_q.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HOST_RSP_OK "\r\nOK\r\n"
#define HOST_RSP_OK_PROMPT "\r\nOK\r\n>\r\n"
#define HOST_RSP_ERR "\r\nERROR\r\n"
#define HOST_COLLECT_CAP 4096U

/**
 * @brief 绑定队列
 * @param down 下行
 * @param up   上行
 * @param idle_ms 从机 idle
 */
void host_api_bind(link_q_t *down, link_q_t *up, uint32_t idle_ms);

/**
 * @brief 场景开始：吸干队列并复位命令状态
 */
void host_scene_begin(void);

/**
 * @brief 明文发送
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
 * @brief 限时收集上行
 * @param out     缓冲
 * @param out_cap 容量
 * @param wait_ms 等待
 * @return 字节数
 */
uint32_t host_collect(char *out, uint32_t out_cap, uint32_t wait_ms);

/**
 * @brief 默认 settle 收集
 * @param out     缓冲
 * @param out_cap 容量
 * @return 字节数
 */
uint32_t host_collect_settle(char *out, uint32_t out_cap);

/**
 * @brief 等到上行出现子串或超时
 * @param out     缓冲
 * @param out_cap 容量
 * @param needle  子串
 * @param wait_ms 超时
 * @return 已收集字节数
 */
uint32_t host_wait_contains(char *out, uint32_t out_cap, const char *needle,
                            uint32_t wait_ms);

/**
 * @brief 等到上行整段精确等于 expect 或超时
 * @param out     缓冲
 * @param out_cap 容量
 * @param expect  期望全文
 * @param wait_ms 超时
 * @return 已收集字节数
 */
uint32_t host_wait_exact(char *out, uint32_t out_cap, const char *expect,
                         uint32_t wait_ms);

/**
 * @brief 安静窗：期间上行须保持无新数据
 * @param wait_ms 观察时长
 * @return 1 全程安静；0 期间收到字节
 */
int host_expect_quiet(uint32_t wait_ms);

/**
 * @brief 统计子串出现次数
 * @param hay    被搜
 * @param needle 子串
 * @return 次数
 */
uint32_t host_count_substr(const char *hay, const char *needle);

/**
 * @brief 断言
 * @param ok  非 0 通过
 * @param msg 说明
 */
void host_check(int ok, const char *msg);

/**
 * @brief 取断言计数
 * @param total 总数；可为 NULL
 * @param fail  失败；可为 NULL
 */
void host_check_stats(uint32_t *total, uint32_t *fail);

/**
 * @brief 默认 settle ms
 * @return 毫秒
 */
uint32_t host_settle_ms(void);

/**
 * @brief 合法 +++ 退出流式（前后静默）
 */
void host_exit_stream_plus(void);

/**
 * @brief 合法 +++ 退出流式并消费退出回包 \r\nOK\r\n
 *
 * 流式退出成功后库默认自动回 \r\nOK\r\n，本函数退出后等待并断言该回包，
 * 避免与后续命令应答粘连或误入安静窗断言。
 * @param tag 断言标签
 */
void host_exit_stream_ok(const char *tag);

/**
 * @brief CIPMODE=1 后无参 CIPSEND，校验提示符
 * @param tag 断言标签
 * @return 0 成功进入；非 0 失败
 */
int host_enter_stream(const char *tag);

/**
 * @brief CIPSEND=<len>，校验提示符
 * @param len 长度
 * @param tag 断言标签
 * @return 0 成功；非 0 失败
 */
int host_enter_fixed(uint32_t len, const char *tag);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_SEND_HOST_API_H */
