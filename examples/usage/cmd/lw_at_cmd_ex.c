/**
 * @file lw_at_cmd_ex.c
 * @brief LW-AT 精简例程命令表实现（非库核心）
 *
 * @details
 * AT、AT+INT、AT+STR、AT+MIX、AT+SLOT。开启数据模式时另含应用侧
 * AT+CIPMODE 与 AT+CIPSEND（带长度=定长；无参且 CIPMODE=1=流式），
 * 对齐 ESP-AT 用法；命令名仅存在于本例程，非库内置。
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
#include "lw_at_cmd_ex.h"

#include <string.h>

/* 字符串参数最大保存长度（含 NUL） */
#define CMD_EX_STR_CAP 32U

/* AT+MIX 至少两槽：整数 + 字符串 */
#define CMD_EX_MIX_PARA_MIN 2U

/* AT+SLOT 固定三槽：左整数、中间空槽、右整数 */
#define CMD_EX_SLOT_PARA_NUM 3U

/* 例程定长接收缓冲容量（含 NUL） */
#define CMD_EX_CIPSEND_CAP 64U

/* 应用侧传输模式：0 普通；1 允许无参 CIPSEND 进流式（非内核状态） */
#define CMD_EX_CIPMODE_NORMAL 0
#define CMD_EX_CIPMODE_PASS   1

static int32_t int_val;
static char str_val[CMD_EX_STR_CAP];
static int32_t mix_a;
static char mix_str[CMD_EX_STR_CAP];
static int32_t slot_left;
static int32_t slot_right;

static char cipsend_buf[CMD_EX_CIPSEND_CAP];
static uint32_t cipsend_len;
static int32_t cipmode;

/**
 * @brief 裸 AT：联通性检查
 * @param ctx 未使用
 * @return LW_AT_OK
 */
static at_rc_t cmd_ex_at_exe(void *ctx)
{
    (void)ctx;
    return LW_AT_OK;
}

/**
 * @brief AT+INT=?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_int_test(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+INT:(int32)") >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+INT?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_int_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+INT:%ld", (long)int_val) >= 0) ? LW_AT_OK
                                                             : LW_AT_ERROR;
}

/**
 * @brief AT+INT=<n>
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t cmd_ex_int_setup(uint8_t para_num, void *ctx)
{
    int32_t val;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &val) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    int_val = val;
    return LW_AT_OK;
}

/**
 * @brief AT+STR=?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_str_test(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+STR:(string)") >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+STR?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_str_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+STR:%s", str_val) >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+STR=<s>
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t cmd_ex_str_setup(uint8_t para_num, void *ctx)
{
    const char *s;
    size_t n;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_str(0U, &s) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    n = strlen(s);
    if (n >= CMD_EX_STR_CAP) {
        return LW_AT_ERROR;
    }
    (void)memcpy(str_val, s, n + 1U);
    return LW_AT_OK;
}

/**
 * @brief AT+MIX=?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_mix_test(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+MIX:(int32),(string)") >= 0) ? LW_AT_OK
                                                           : LW_AT_ERROR;
}

/**
 * @brief AT+MIX?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_mix_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+MIX:%ld,%s", (long)mix_a, mix_str) >= 0)
               ? LW_AT_OK
               : LW_AT_ERROR;
}

/**
 * @brief AT+MIX=<int>,<str>
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t cmd_ex_mix_setup(uint8_t para_num, void *ctx)
{
    const char *s;
    size_t n;
    int32_t a;

    (void)ctx;
    if (para_num < CMD_EX_MIX_PARA_MIN) {
        return LW_AT_ERROR;
    }
    if (lw_at_get_para_digit(0U, &a) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if (lw_at_get_para_str(1U, &s) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    n = strlen(s);
    if (n >= CMD_EX_STR_CAP) {
        return LW_AT_ERROR;
    }
    mix_a = a;
    (void)memcpy(mix_str, s, n + 1U);
    return LW_AT_OK;
}

/**
 * @brief AT+SLOT=?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_slot_test(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+SLOT:(int32),,(int32)") >= 0) ? LW_AT_OK
                                                            : LW_AT_ERROR;
}

/**
 * @brief AT+SLOT?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_slot_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+SLOT:%ld,,%ld", (long)slot_left,
                            (long)slot_right) >= 0)
               ? LW_AT_OK
               : LW_AT_ERROR;
}

/**
 * @brief AT+SLOT=<left>,,<right>：中间必须为空槽
 * @param para_num 槽数（须为 3）
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t cmd_ex_slot_setup(uint8_t para_num, void *ctx)
{
    int32_t left;
    int32_t right;
    const char *mid;
    lw_at_para_rc_t mid_rc;

    (void)ctx;
    if (para_num != CMD_EX_SLOT_PARA_NUM) {
        return LW_AT_ERROR;
    }
    if (lw_at_get_para_digit(0U, &left) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    mid_rc = lw_at_get_para_str(1U, &mid);
    (void)mid;
    if (mid_rc != LW_AT_PARA_OMITTED) {
        return LW_AT_ERROR;
    }
    if (lw_at_get_para_digit(2U, &right) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    slot_left = left;
    slot_right = right;
    return LW_AT_OK;
}

/**
 * @brief AT+CIPMODE?：查询应用侧传输模式
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t cmd_ex_cipmode_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+CIPMODE:%ld", (long)cipmode) >= 0) ? LW_AT_OK
                                                                 : LW_AT_ERROR;
}

/**
 * @brief AT+CIPMODE=<0|1>：仅保存应用配置，不切入数据模式
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t cmd_ex_cipmode_setup(uint8_t para_num, void *ctx)
{
    int32_t mode;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &mode) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if ((mode != CMD_EX_CIPMODE_NORMAL) && (mode != CMD_EX_CIPMODE_PASS)) {
        return LW_AT_ERROR;
    }
    cipmode = mode;
    return LW_AT_OK;
}

/**
 * @brief 定长分片：写入例程缓冲（演示「假发送」落盘）
 */
static void cmd_ex_cipsend_chunk(const uint8_t *data, uint32_t len, void *user)
{
    uint32_t i;

    (void)user;
    for (i = 0U; i < len; i++) {
        if (cipsend_len + 1U >= CMD_EX_CIPSEND_CAP) {
            break;
        }
        cipsend_buf[cipsend_len] = (char)data[i];
        cipsend_len++;
    }
    cipsend_buf[cipsend_len] = '\0';
}

/**
 * @brief 定长收满：业务钩子（最终 OK 由库统一回）
 */
static void cmd_ex_cipsend_done(uint32_t got, void *user)
{
    (void)got;
    (void)user;
}

/**
 * @brief AT+CIPSEND=<len>：普通定长收数（不依赖 CIPMODE），handler 内同步确认
 * @param para_num 槽数
 * @param ctx      未使用
 * @return LW_AT_OK 或 LW_AT_ERROR
 * @note 同步场景：handler 内调 lw_at_data_confirm 切模式，> 由 core_exec_line 补打
 */
static at_rc_t cmd_ex_cipsend_setup(uint8_t para_num, void *ctx)
{
    int32_t len;
    lw_at_data_enter_cfg_t cfg;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &len) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if ((len <= 0) || ((uint32_t)len >= CMD_EX_CIPSEND_CAP)) {
        return LW_AT_ERROR;
    }
    cipsend_len = 0U;
    cipsend_buf[0] = '\0';
    memset(&cfg, 0, sizeof(cfg));
    cfg.policy = LW_AT_DATA_FIXED;
    cfg.length = (uint32_t)len;
    cfg.on_chunk = cmd_ex_cipsend_chunk;
    cfg.on_done = cmd_ex_cipsend_done;
    if (lw_at_data_enter(&cfg) != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    /* 同步确认：参数校验通过后立即切入，> 由 core_exec_line 补打 */
    (void)lw_at_data_confirm();
    return LW_AT_OK;
}

/**
 * @brief AT+CIPSEND：无参；仅当 CIPMODE=1 时登记进入流式透传（异步确认）
 * @param ctx 未使用
 * @return LW_AT_OK 或 LW_AT_ERROR
 * @note 异步场景：仅登记 enter_req，不调 confirm；> 由外部 lw_at_data_confirm 打印
 */
static at_rc_t cmd_ex_cipsend_exe(void *ctx)
{
    (void)ctx;
    if (cipmode != CMD_EX_CIPMODE_PASS) {
        return LW_AT_ERROR;
    }
    /* 仅登记意图，外部等待网络就绪后再调 lw_at_data_confirm */
    if (lw_at_transmit_enter() != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    return LW_AT_OK;
}

static const lw_at_cmd_t cmd_ex_cmds[] = {
    { "",     NULL,            NULL,             NULL,             cmd_ex_at_exe },
    { "+INT", cmd_ex_int_test, cmd_ex_int_query, cmd_ex_int_setup, NULL },
    { "+STR", cmd_ex_str_test, cmd_ex_str_query, cmd_ex_str_setup, NULL },
    { "+MIX", cmd_ex_mix_test, cmd_ex_mix_query, cmd_ex_mix_setup, NULL },
    { "+SLOT", cmd_ex_slot_test, cmd_ex_slot_query, cmd_ex_slot_setup, NULL },
    { "+CIPMODE", NULL, cmd_ex_cipmode_query, cmd_ex_cipmode_setup, NULL },
    { "+CIPSEND", NULL, NULL, cmd_ex_cipsend_setup, cmd_ex_cipsend_exe },
};

static lw_at_cmd_table_t cmd_ex_table = {
    cmd_ex_cmds,
    (uint16_t)(sizeof(cmd_ex_cmds) / sizeof(cmd_ex_cmds[0])),
    NULL,
};

/**
 * @brief 注册例程命令表
 */
lw_at_err_t lw_at_cmd_ex_register(void)
{
    /* deinit/init 后 next 可能残留，注册前清零 */
    cmd_ex_table.next = NULL;
    return lw_at_cmd_register(&cmd_ex_table);
}
