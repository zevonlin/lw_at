/**
 * @file slave_rt.h
 * @brief host_slave_send 从机运行时
 *
 * @details
 * 独立线程跑 lw_at；提供透传 sink、应用侧 data_exit 侧信道。
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
#ifndef HOST_SLAVE_SEND_SLAVE_RT_H
#define HOST_SLAVE_SEND_SLAVE_RT_H

#include "link_q.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从机启动参数
 */
typedef struct {
    uint32_t idle_ms;       /**< 空闲门控；0 用默认 */
    uint32_t rx_buf_size;   /**< 0 用默认 */
    uint32_t line_buf_size; /**< 0 用默认 */
    uint32_t guard_ms;      /**< +++ 静默；0 用默认 */
} slave_rt_cfg_t;

/**
 * @brief 启动从机线程
 * @param down 下行队列
 * @param up   上行队列
 * @param cfg  配置；可为 NULL
 * @return 0 成功
 */
int slave_rt_start(link_q_t *down, link_q_t *up, const slave_rt_cfg_t *cfg);

/**
 * @brief 停止从机
 */
void slave_rt_stop(void);

/**
 * @brief 停线程后按新配置重启
 * @param cfg 新配置
 * @return 0 成功
 */
int slave_rt_reinit(const slave_rt_cfg_t *cfg);

/**
 * @brief 透传 sink 捕获（NUL 结尾）
 * @return 缓冲
 */
const char *slave_rt_sink_get(void);

/**
 * @brief sink 长度
 * @return 字节数
 */
uint32_t slave_rt_sink_len(void);

/**
 * @brief 清空 sink
 */
void slave_rt_sink_clear(void);

/**
 * @brief 请求从机线程调用 lw_at_data_exit（侧信道）
 */
void slave_rt_request_data_exit(void);

/**
 * @brief 当前配置的 guard_ms
 * @return 毫秒
 */
uint32_t slave_rt_guard_ms(void);

/**
 * @brief 当前配置的 idle_ms
 * @return 毫秒
 */
uint32_t slave_rt_idle_ms(void);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_SEND_SLAVE_RT_H */
