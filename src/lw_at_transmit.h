/**
 * @file lw_at_transmit.h
 * @brief LW-AT 透传模块接口：sink 转发与 +++ 静默守卫检测
 *
 * @details
 * 透传独立状态结构体与操作函数；只依赖 lw_at_stream，不持有也不修改
 * 工作模式，退出判定经返回值/输出参数上报，由上层 core 统一切换模式
 * 并排空缓存。前后静默由上层 guard 定时器置 silent 标志判定。
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
#ifndef LW_AT_TRANSMIT_H
#define LW_AT_TRANSMIT_H

#include "lw_at.h"
#include "lw_at_stream.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 透传状态：静默守卫配置与 +++ 序列暂存计数
 */
typedef struct {
    uint32_t guard_ms;         /**< +++ 前后静默时长；0 表示关闭守卫 */
    volatile uint8_t plus_cnt; /**< 退出序列已暂存的 '+' 个数 */
} lw_at_transmit_t;

/**
 * @brief 静态初始化透传状态（不做任何动态内存分配）
 * @param tm       透传实例
 * @param guard_ms +++ 前后静默时长（ms），0 表示关闭守卫
 */
void lw_at_transmit_init(lw_at_transmit_t *tm, uint32_t guard_ms);

/**
 * @brief 复位透传状态（清空 +++ 暂存计数）
 * @param tm 透传实例
 */
void lw_at_transmit_reset(lw_at_transmit_t *tm);

/**
 * @brief 透传 feed 路径：+++ 守卫检测，普通数据写入 stream
 *
 * 候选 '+' 先暂存不入缓存，序列被破坏时还原写回；已凑齐 +++ 且
 * silent 为 1 时，置 *exited=1 并复位 plus_cnt，本段字节不写入
 * stream（由上层在记下排空分界后按命令数据写入）。silent 为 0 时
 * 暂存的 '+' 还原为普通数据。本函数不做解析与 write，允许 ISR 上下文。
 * @param tm     透传实例
 * @param stream 接收流实例
 * @param data   数据起始地址
 * @param len    数据长度
 * @param silent 1 表示 guard 定时器已到期（静默满足）；0 未满足
 * @param exited 输出：1 表示后静默已确认、应退出透传；否则 0
 * @return 实际接受的字节数（含暂存的 '+'）；*exited=1 时本段未入缓存，
 *         返回 0，由上层继续写入
 */
int32_t lw_at_transmit_feed(lw_at_transmit_t *tm, lw_at_stream_t *stream,
                            const uint8_t *data, uint32_t len, uint8_t silent,
                            uint8_t *exited);

/**
 * @brief 透传 process 路径：把 stream 中数据全部转发给 sink
 *
 * 透传下缓存满只丢弃不回 ERROR（避免污染用户数据流），满标志在此清除。
 * @param stream    接收流实例
 * @param sink      透传下行回调
 * @param sink_user sink 的用户指针
 */
void lw_at_transmit_process(lw_at_stream_t *stream, lw_at_sink_cb_t sink,
                            void *sink_user);

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_TRANSMIT_H */
