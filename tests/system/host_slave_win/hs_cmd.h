/**
 * @file hs_cmd.h
 * @brief 主机↔从机测例专用 AT 命令集（非库核心）
 *
 * @details
 * 提供 PING/INT/STR/MIX/MULTI/FAIL/SLOW/TRANS，便于双端场景断言。
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
#ifndef HOST_SLAVE_HS_CMD_H
#define HOST_SLAVE_HS_CMD_H

#include "lw_at.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* AT+STR 内部缓冲（含 NUL） */
#define HS_CMD_STR_CAP 64U

/**
 * @brief 注册测试命令表
 * @return LW_AT_ERR_OK 成功
 */
lw_at_err_t hs_cmd_register(void);

/**
 * @brief 复位命令状态
 */
void hs_cmd_reset_state(void);

/**
 * @brief 取 INT 当前值
 * @return 整型值
 */
int32_t hs_cmd_int_get(void);

/**
 * @brief 取 STR 当前串
 * @return 静态缓冲指针
 */
const char *hs_cmd_str_get(void);

/**
 * @brief 取 MIX 状态
 * @param a           输出整型
 * @param str         输出串缓冲
 * @param str_cap     缓冲容量
 * @param opt         输出可选整型
 * @param opt_present 1 表示第三槽存在
 */
void hs_cmd_mix_get(int32_t *a, char *str, uint32_t str_cap, int32_t *opt,
                    uint8_t *opt_present);

/**
 * @brief 取 MULTI 三整数
 * @param a 输出
 * @param b 输出
 * @param c 输出
 */
void hs_cmd_multi_get(int32_t *a, int32_t *b, int32_t *c);

/**
 * @brief 设置 SLOW 延时
 * @param ms 毫秒
 */
void hs_cmd_slow_set_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_HS_CMD_H */
