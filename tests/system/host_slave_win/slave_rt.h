/**
 * @file slave_rt.h
 * @brief 从机运行时：独立线程跑 lw_at
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
 * Date       Author    Notes                              version
 * 2026-07-30 linzhiwei 小缓存 reinit、透传 sink           v1.1.0
 * 2026-07-30 linzhiwei 首次发布                            v1.0.0
 */
#ifndef HOST_SLAVE_SLAVE_RT_H
#define HOST_SLAVE_SLAVE_RT_H

#include "link_q.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 从机启动参数
 */
typedef struct {
    uint32_t idle_ms;      /**< 空闲门控 */
    uint32_t rx_buf_size;  /**< 0 表示默认 */
    uint32_t line_buf_size;/**< 0 表示默认 */
    uint32_t guard_ms;     /**< 透传守卫；0 用默认 100 */
} slave_rt_cfg_t;

/**
 * @brief 启动从机线程
 * @param down 下行队列
 * @param up   上行队列
 * @param cfg  配置；可为 NULL 用默认
 * @return 0 成功
 */
int slave_rt_start(link_q_t *down, link_q_t *up, const slave_rt_cfg_t *cfg);

/**
 * @brief 停止从机并反初始化
 */
void slave_rt_stop(void);

/**
 * @brief 停线程后按新配置重启（测小缓存）
 * @param cfg 新配置
 * @return 0 成功
 */
int slave_rt_reinit(const slave_rt_cfg_t *cfg);

/**
 * @brief 取透传 sink 捕获（NUL 结尾）
 * @return 缓冲指针
 */
const char *slave_rt_sink_get(void);

/**
 * @brief 取 sink 长度
 * @return 字节数
 */
uint32_t slave_rt_sink_len(void);

/**
 * @brief 清空 sink
 */
void slave_rt_sink_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_SLAVE_RT_H */
