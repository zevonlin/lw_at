/**
 * @file lw_at_core.h
 * @brief LW-AT 核心模块内部头：工作模式与库实例组合结构体
 *
 * @details
 * 定义库实例结构体 lw_at_t：聚合配置、接收流与数据模式状态。
 * 实例本体为 lw_at_core.c 内的静态单例。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 0.9.1
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-11 linzhiwei 精简头注释 @details 至职责/约束/依赖          v0.9.1
 * 2026-08-06 linzhiwei 首次发布                                    v0.9.0
 */
#ifndef LW_AT_CORE_H
#define LW_AT_CORE_H

#include "lw_at.h"
#include "lw_at_stream.h"
#include "lw_at_transmit.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 工作模式
 */
typedef enum {
    LW_AT_MODE_COMMAND = 0, /**< 命令模式：按 \r\n 拆行、查表执行 */
    LW_AT_MODE_DATA = 1     /**< 数据模式：定长窗口或流式透传 */
} lw_at_mode_t;

/* 兼容旧内部命名：流式透传即数据模式之一 */
#define LW_AT_MODE_TRANSMIT LW_AT_MODE_DATA

/**
 * @brief 库实例：配置 + 各模块状态的组合
 */
typedef struct {
    uint8_t inited;                  /**< 是否已初始化 */
    volatile uint8_t mode;           /**< 当前模式，取值见 lw_at_mode_t */
    lw_at_config_t cfg;              /**< 初始化时拷贝的配置 */
    lw_at_stream_t stream;           /**< 接收流状态 */
    lw_at_transmit_t transmit;       /**< +++ 守卫状态（流式 / 可中止定长） */
    uint8_t data_policy;             /**< 当前或待进入策略，见 lw_at_data_policy_t */
    uint32_t data_length;            /**< FIXED 目标长度 */
    uint32_t data_got;               /**< FIXED 已交付字节数 */
    uint32_t data_guard_ms;          /**< 本会话 guard_ms */
    uint8_t allow_plus_abort;        /**< FIXED 是否允许 +++ 取消 */
    lw_at_data_chunk_cb_t on_chunk;  /**< 本会话分片回调 */
    lw_at_data_done_cb_t on_done;    /**< 本会话完成回调 */
    void *data_user;                 /**< 本会话用户指针 */
    uint8_t enter_req;               /**< handler 请求进入数据模式的登记标志 */
    volatile uint8_t exit_req;       /**< 待退出：由 feed/定时器回调置位，主路径完成 */
    uint32_t exit_mark;              /**< 退出时 stream->head 快照：此前字节属数据 */
    volatile uint8_t silent;         /**< guard 定时器已到期标志：1=静默满足；feed 时清零 */
    const char *args[LW_AT_ARG_MAX]; /**< setup 槽指针表，指向 line_buf 内已拆正文 */
    uint8_t arg_num;                 /**< 本次 setup 槽个数 */
} lw_at_t;

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_CORE_H */
