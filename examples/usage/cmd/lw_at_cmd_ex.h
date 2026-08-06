/**
 * @file lw_at_cmd_ex.h
 * @brief LW-AT 精简例程命令表接口（非库核心）
 *
 * @details
 * 仅演示：裸 AT、INT、STR、MIX、SLOT、CIPMODE、CIPSEND（定长/流式）。
 * 测试命令表见 tests/fixtures/test_cmd。
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
 * Date       Author    Notes                              version
 * 2026-08-06 linzhiwei 首次发布                            v0.9.0
 */
#ifndef LW_AT_CMD_EX_H
#define LW_AT_CMD_EX_H

#include "lw_at.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将例程命令表注册到字典
 * @return LW_AT_ERR_OK 成功；否则为错误码
 */
lw_at_err_t lw_at_cmd_ex_register(void);

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_CMD_EX_H */
