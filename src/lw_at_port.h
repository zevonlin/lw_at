/**
 * @file lw_at_port.h
 * @brief LW-AT 移植层：编译配置、C 库符号映射与定时器回调类型
 *
 * @details
 * 集中放置可由产品侧覆盖的配置宏（如 LW_AT_ARG_MAX），以及标准 C 库
 * 函数的小写下划线别名（如 lw_at_memset）。默认映射到宿主 C 库；嵌入式
 * 环境可在包含本头文件之前自行 #define 以替换实现。
 * 数据模式（流式透传与定长收数）恒编译，不设裁剪开关；流式路径在
 * 未注册 sink 时运行时校验失败。
 * 本文件亦定义 lw_at_timer_cb_t（单次软件定时器到期回调），供板级
 * timer_arm/timer_stop 注册使用。板级 write 与定时器操作定义于
 * lw_at_port_ops_t（见 lw_at.h）；例程见 examples/port，测试见
 * tests/fixtures/test_port。
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
#ifndef LW_AT_PORT_H
#define LW_AT_PORT_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* setup 命令参数最大个数，可通过编译选项或包含前宏覆盖 */
#ifndef LW_AT_ARG_MAX
#define LW_AT_ARG_MAX 8U
#endif

#ifndef lw_at_memset
#define lw_at_memset memset
#endif

#ifndef lw_at_memcmp
#define lw_at_memcmp memcmp
#endif

#ifndef lw_at_memcpy
#define lw_at_memcpy memcpy
#endif

#ifndef lw_at_strcmp
#define lw_at_strcmp strcmp
#endif

#ifndef lw_at_strncmp
#define lw_at_strncmp strncmp
#endif

#ifndef lw_at_strlen
#define lw_at_strlen strlen
#endif

#ifndef lw_at_strtol
#define lw_at_strtol strtol
#endif

#ifndef lw_at_snprintf
#define lw_at_snprintf snprintf
#endif

#ifndef lw_at_vsnprintf
#define lw_at_vsnprintf vsnprintf
#endif

#ifndef lw_at_va_list
#define lw_at_va_list va_list
#endif

#ifndef lw_at_va_start
#define lw_at_va_start va_start
#endif

#ifndef lw_at_va_end
#define lw_at_va_end va_end
#endif

/**
 * @brief 单次软件定时器到期回调（可能在 ISR 上下文中执行）
 *
 * 由板级 timer_arm 注册，在定时器到期时调用。回调内只可置标志，
 * 不可执行 AT 解析或 port write。user 为 timer_arm 传入的指针。
 * @param user timer_arm 时传入的用户指针
 */
typedef void (*lw_at_timer_cb_t)(void *user);

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_PORT_H */
