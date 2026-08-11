/**
 * @file lw_at_cmd_dict.h
 * @brief LW-AT 命令字典模块接口：链表注册与按名查找
 *
 * @details
 * 命令字典：哨兵头结点链表注册与按名查找，调用方无需传递链表头。
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
#ifndef LW_AT_CMD_DICT_H
#define LW_AT_CMD_DICT_H

#include "lw_at.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 复位字典：清空哨兵头结点后的链表（不释放用户节点）
 */
void lw_at_cmd_dict_reset(void);

/**
 * @brief 将一张命令表节点挂到字典链表
 * @param table 用户静态节点
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_PARAM 非法或重名；
 *         LW_AT_ERR_STATE 节点已在链上
 */
lw_at_err_t lw_at_cmd_dict_register(lw_at_cmd_table_t *table);

/**
 * @brief 按名称在全部已注册表中精确查找（大小写敏感）
 * @param name 命令名，如 "+ECHO"；裸 AT 用 ""
 * @return 命中的命令描述符；未找到返回 NULL
 */
const lw_at_cmd_t *lw_at_cmd_dict_find(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_CMD_DICT_H */
