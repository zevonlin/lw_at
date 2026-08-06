/**
 * @file test_cmd.h
 * @brief 测试用命令表：AT、ECHO、可选 CIPMODE/CIPSEND
 *
 * @details
 * 供 tests 注册与断言。基础表含裸 AT 与 AT+ECHO（四种形态）；
 * 数据模式表含 AT+CIPMODE 与 AT+CIPSEND（定长/流式，对齐 ESP-AT
 * 用法），恒编译。产品例程见 examples/cmd。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 2.3.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                              version
 * 2026-08-01 linzhiwei 移除 LW_AT_CFG_TRANSMIT 条件编译     v2.3.0
 * 2026-07-31 linzhiwei TRANS/SEND 改为 CIPMODE+CIPSEND      v2.2.0
 * 2026-07-31 linzhiwei 增加 AT+SEND 定长收数与查询接口       v2.1.0
 * 2026-07-30 linzhiwei 由 cmd_demo 重命名为 test_cmd       v2.0.0
 * 2026-07-30 linzhiwei 迁入 tests/fixtures                 v1.2.0
 * 2026-07-30 linzhiwei 首次发布                            v1.0.0
 */
#ifndef TEST_CMD_H
#define TEST_CMD_H

#include "lw_at.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 将测试命令表全部注册到字典（基础表 + 数据模式表）
 * @return LW_AT_ERR_OK 全部成功；否则为首次失败的错误码
 */
lw_at_err_t test_cmd_register(void);

/**
 * @brief 取基础命令表节点（裸 AT、+ECHO），供单独注册或测试
 * @return 静态节点指针
 */
lw_at_cmd_table_t *test_cmd_basic_table(void);

/**
 * @brief 取数据模式命令表节点（+CIPMODE/+CIPSEND）
 * @return 静态节点指针
 */
lw_at_cmd_table_t *test_cmd_trans_table(void);

/**
 * @brief 取定长 CIPSEND 已捕获正文（供测试断言）
 * @return NUL 结尾字符串
 */
const char *test_cmd_send_buf_get(void);

/**
 * @brief 取定长 CIPSEND 完成回调收到的字节数
 * @return got
 */
uint32_t test_cmd_send_done_got(void);

/**
 * @brief 取 AT+ECHO 当前保存的值（供测试断言）
 * @return 当前 ECHO 值
 */
int32_t test_cmd_echo_get(void);

/**
 * @brief 取最近一次 AT+ECHO 设置命令的参数个数（供测试断言）
 * @return 参数个数
 */
uint8_t test_cmd_para_num_get(void);

#ifdef __cplusplus
}
#endif

#endif /* TEST_CMD_H */
