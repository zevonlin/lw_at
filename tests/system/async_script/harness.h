/**
 * @file harness.h
 * @brief 异步事件脚本测试框架接口（非库核心）
 *
 * @details
 * 用 C 步骤表描述 FEED/TICK/PROCESS/期望，在虚拟时钟上复现
 * 「收包中断随时喂数、定时器到期触发回调」的交错，便于回归复现。
 * 本 harness 刻意单线程、确定性交错；不模拟多核抢占 lw_at API
 * （库契约为不具备线程安全）。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 1.2.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei TICK 改为推进虚拟时钟触发定时器回调          v1.2.0
 * 2026-07-30 linzhiwei 增加 H_EXPECT_OUT_COUNT                    v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
 */
#ifndef ASYNC_SCRIPT_HARNESS_H
#define ASYNC_SCRIPT_HARNESS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 脚本步骤类型
 */
typedef enum {
    H_END = 0,          /**< 脚本结束 */
    H_INFO,             /**< 打印说明，p 为文案 */
    H_FEED,             /**< 喂入 NUL 结尾字符串，p 为 const char * */
    H_FEED_BIN,         /**< 喂入二进制，p 为字节指针，u 为长度 */
    H_TICK,             /**< 推进虚拟时钟 u 毫秒（触发到期的定时器回调） */
    H_PROCESS,          /**< 调用 lw_at_process */
    H_OUT_CLEAR,        /**< 清空主机侧输出捕获 */
    H_EXPECT_OUT,       /**< 输出须与 p 完全一致 */
    H_EXPECT_OUT_HAS,   /**< 输出须包含子串 p */
    H_EXPECT_OUT_COUNT, /**< 子串 p 在输出中出现次数须等于 u */
    H_EXPECT_FN         /**< p 为 int (*)(void)，非 0 通过（自定义断言） */
} h_ev_t;

/**
 * @brief 单步事件
 */
typedef struct {
    h_ev_t type;     /**< 事件类型 */
    uint32_t u;      /**< TICK 的 Δms，FEED_BIN 长度，或 OUT_COUNT 期望次数 */
    const void *p;   /**< 字符串/二进制/说明/期望，视 type 而定 */
} h_step_t;

/**
 * @brief 场景开始前的库初始化回调
 */
typedef void (*h_setup_fn)(void);

/**
 * @brief 运行一条事件脚本
 * @param name  场景名（打印用）
 * @param setup 场景前初始化；可为 NULL
 * @param steps 以 H_END 结尾的步骤表
 * @return 0 全部期望通过；1 有失败
 */
int harness_run(const char *name, h_setup_fn setup, const h_step_t *steps);

/**
 * @brief 取 harness 累计断言失败数（跨场景累加）
 * @return 失败次数
 */
uint32_t harness_fail_count(void);

/**
 * @brief 取 harness 累计断言次数
 * @return 断言次数
 */
uint32_t harness_check_count(void);

#ifdef __cplusplus
}
#endif

#endif /* ASYNC_SCRIPT_HARNESS_H */
