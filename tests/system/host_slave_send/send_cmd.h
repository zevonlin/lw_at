/**
 * @file send_cmd.h
 * @brief host_slave_send 专题命令表：CIPMODE / CIPABORT / CIPSEND
 *
 * @details
 * 应用侧 CIPMODE/CIPABORT；定长捕获供主机侧断言。
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
#ifndef HOST_SLAVE_SEND_CMD_H
#define HOST_SLAVE_SEND_CMD_H

#include "lw_at.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 注册专题命令表
 * @return LW_AT_ERR_OK 或错误码
 */
lw_at_err_t send_cmd_register(void);

/**
 * @brief 复位 CIPMODE/CIPABORT 与定长捕获
 */
void send_cmd_reset_state(void);

/**
 * @brief 定长捕获正文（NUL 结尾）
 * @return 缓冲指针
 */
const char *send_cmd_fixed_buf_get(void);

/**
 * @brief 定长已捕获长度
 * @return 字节数
 */
uint32_t send_cmd_fixed_len_get(void);

/**
 * @brief 最近一次 on_done 的 got
 * @return 字节数
 */
uint32_t send_cmd_fixed_done_got(void);

/**
 * @brief 当前应用侧 CIPMODE
 * @return 0 或 1
 */
int32_t send_cmd_cipmode_get(void);

/**
 * @brief 当前 CIPABORT 开关
 * @return 0 或 1
 */
int32_t send_cmd_cipabort_get(void);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_SEND_CMD_H */
