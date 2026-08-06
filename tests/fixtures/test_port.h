/**
 * @file test_port.h
 * @brief 测试用 Port：应答捕获与虚拟单次软件定时器
 *
 * @details
 * 供 tests 链接。write 将从机上行追加到内存缓冲；timer_arm/timer_stop
 * 登记单次软件定时器，test_port_tick_advance 推进虚拟时钟并在定时器
 * 到期时触发其回调，从而精确控制空闲与透传静默（等价硬件定时器 ISR）。
 * 产品例程请使用 examples/port。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 3.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei 事件驱动改造：移除 get_tick_ms，新增 timer   v3.0.0
 * 2026-07-30 linzhiwei 由 port_pc 重命名为 test_port               v2.0.0
 * 2026-07-30 linzhiwei 迁入 tests/fixtures                         v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
 */
#ifndef TEST_PORT_H
#define TEST_PORT_H

#include <stdint.h>

#include "lw_at_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 输出捕获缓冲字节数（含结尾 NUL 预留） */
#ifndef TEST_PORT_OUT_BUF_SIZE
#define TEST_PORT_OUT_BUF_SIZE 1024U
#endif

/**
 * @brief Port write：把从机上行数据追加到捕获缓冲
 * @param data 数据起始地址
 * @param len  数据长度
 * @return 实际写入字节数（本实现总是等于 len；缓冲满时丢弃多余字节）
 */
int32_t test_port_write(const uint8_t *data, int32_t len);

/**
 * @brief 启动/重载单次软件定时器（虚拟实现，供测试精确控制时间）
 *
 * 记录到期时刻；重复调用覆盖前次。到期回调由 test_port_tick_advance
 * 在推进虚拟时钟时触发（仅触发一次）。
 * @param ms   定时时长（ms）
 * @param cb   到期回调
 * @param user 回调用户指针
 * @return 0 成功；-1 失败
 */
int32_t test_port_timer_arm(uint32_t ms, lw_at_timer_cb_t cb, void *user);

/**
 * @brief 停止定时器，已 arm 的不再触发
 */
void test_port_timer_stop(void);

/**
 * @brief 推进虚拟时钟；若已登记的单次定时器到期则触发其回调
 * @param ms 推进的毫秒数
 */
void test_port_tick_advance(uint32_t ms);

/**
 * @brief 读取当前捕获的全部输出（NUL 结尾）
 * @return 捕获缓冲字符串
 */
const char *test_port_out_get(void);

/**
 * @brief 清空输出捕获缓冲
 */
void test_port_out_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_PORT_H */
