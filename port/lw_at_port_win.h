/**
 * @file lw_at_port_win.h
 * @brief LW-AT Windows 例程适配层接口（非库核心）
 *
 * @details
 * 面向 MCU 风格主循环演示：write 打到控制台；timer_arm/timer_stop
 * 使用 Windows 线程池定时器（CreateTimerQueueTimer）实现单次软件
 * 定时器，到期后由线程池回调触发，无需主循环轮询。
 * 测试用虚拟时钟与应答捕获见 tests/fixtures/test_port。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-06
 * @version 0.9.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-06 linzhiwei 首次发布                                    v0.9.0
 */
#ifndef LW_AT_PORT_WIN_H
#define LW_AT_PORT_WIN_H

#include <stdint.h>

#include "lw_at_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Port write：把从机上行数据写到标准输出
 * @param data 数据起始地址
 * @param len  数据长度
 * @return 实际写出字节数；失败返回负数
 */
int32_t port_win_write(const uint8_t *data, int32_t len);

/**
 * @brief 启动/重载单次软件定时器（Windows 线程池实现）
 * @param ms   定时时长（ms）
 * @param cb   到期回调
 * @param user 回调用户指针
 * @return 0 成功；-1 失败
 */
int32_t port_win_timer_arm(uint32_t ms, lw_at_timer_cb_t cb, void *user);

/**
 * @brief 停止定时器，等待已触发的回调完成
 */
void port_win_timer_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_PORT_WIN_H */
