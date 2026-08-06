/**
 * @file lw_at_cmd_dict.c
 * @brief LW-AT 命令字典实现：哨兵头结点链表注册与查找
 *
 * @details
 * 模块内静态哨兵头结点 dict_head，已注册表挂在 dict_head.next 之后。
 * 注册时做表内重名、跨表重名与重复挂链检查；查找沿链表顺序线性扫描。
 * 不使用动态内存，节点存储由调用方提供。
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
#include "lw_at_cmd_dict.h"

/**
 * 哨兵头结点：不代表用户命令表，仅作链表锚点。
 * 已注册表均挂在 dict_head.next 之后；复位时只清空 next。
 */
static lw_at_cmd_table_t dict_head;

/**
 * @brief 校验单张命令表：name 非空、条目数合法、表内无重名
 * @param cmds 命令数组
 * @param num  条目数
 * @return LW_AT_ERR_OK 合法；LW_AT_ERR_PARAM 非法
 */
static lw_at_err_t dict_check_table(const lw_at_cmd_t *cmds, uint16_t num)
{
    uint16_t i;
    uint16_t j;

    if ((cmds == NULL) || (num == 0U)) {
        return LW_AT_ERR_PARAM;
    }
    for (i = 0U; i < num; i++) {
        if (cmds[i].name == NULL) {
            return LW_AT_ERR_PARAM;
        }
    }
    for (i = 0U; i < num; i++) {
        for (j = (uint16_t)(i + 1U); j < num; j++) {
            if (lw_at_strcmp(cmds[i].name, cmds[j].name) == 0) {
                return LW_AT_ERR_PARAM;
            }
        }
    }
    return LW_AT_ERR_OK;
}

/**
 * @brief 判断节点是否已在字典链上（防重复注册成环）
 * @param table 待检查节点
 * @return 1 已在链上；0 不在
 */
static uint8_t dict_node_in_list(const lw_at_cmd_table_t *table)
{
    /* 沿已注册链表遍历的当前节点 */
    const lw_at_cmd_table_t *node;

    for (node = dict_head.next; node != NULL; node = node->next) {
        if (node == table) {
            return 1U;
        }
    }
    return 0U;
}

/**
 * @brief 检查新表中的命令名是否与已注册表冲突
 * @param cmds 新表命令数组
 * @param num  新表条目数
 * @return LW_AT_ERR_OK 无冲突；LW_AT_ERR_PARAM 存在重名
 */
static lw_at_err_t dict_check_cross(const lw_at_cmd_t *cmds, uint16_t num)
{
    /* 已注册链表上的当前表节点 */
    const lw_at_cmd_table_t *node;
    uint16_t i;
    uint16_t j;

    for (node = dict_head.next; node != NULL; node = node->next) {
        for (i = 0U; i < num; i++) {
            for (j = 0U; j < node->num; j++) {
                if (lw_at_strcmp(cmds[i].name, node->cmds[j].name) == 0) {
                    return LW_AT_ERR_PARAM;
                }
            }
        }
    }
    return LW_AT_ERR_OK;
}

/**
 * @brief 复位字典：清空哨兵头结点后的链表
 */
void lw_at_cmd_dict_reset(void)
{
    dict_head.cmds = NULL;
    dict_head.num = 0U;
    dict_head.next = NULL;
}

/**
 * @brief 将一张命令表节点挂到字典链表（头插）
 */
lw_at_err_t lw_at_cmd_dict_register(lw_at_cmd_table_t *table)
{
    if (table == NULL) {
        return LW_AT_ERR_PARAM;
    }
    if (dict_check_table(table->cmds, table->num) != LW_AT_ERR_OK) {
        return LW_AT_ERR_PARAM;
    }
    if (dict_node_in_list(table) == 1U) {
        return LW_AT_ERR_STATE;
    }
    if (dict_check_cross(table->cmds, table->num) != LW_AT_ERR_OK) {
        return LW_AT_ERR_PARAM;
    }

    table->next = dict_head.next;
    dict_head.next = table;
    return LW_AT_ERR_OK;
}

/**
 * @brief 按名称在全部已注册表中精确查找
 */
const lw_at_cmd_t *lw_at_cmd_dict_find(const char *name)
{
    /* 当前正在扫描的命令表节点 */
    const lw_at_cmd_table_t *node;
    uint16_t i;

    if (name == NULL) {
        return NULL;
    }
    for (node = dict_head.next; node != NULL; node = node->next) {
        for (i = 0U; i < node->num; i++) {
            if (lw_at_strcmp(node->cmds[i].name, name) == 0) {
                return &node->cmds[i];
            }
        }
    }
    return NULL;
}
