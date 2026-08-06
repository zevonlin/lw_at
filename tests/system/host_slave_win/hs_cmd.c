/**
 * @file hs_cmd.c
 * @brief 主机↔从机测例专用 AT 命令集实现
 *
 * @details
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 1.2.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei CIPSEND 改为 handler 内同步 confirm          v1.2.0
 * 2026-07-31 linzhiwei TRANS 改为 CIPMODE+CIPSEND                  v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
 */
#include "hs_cmd.h"

#include <string.h>
#include <windows.h>

static int32_t int_val;
static char str_val[HS_CMD_STR_CAP];
static int32_t mix_a;
static char mix_str[HS_CMD_STR_CAP];
static int32_t mix_opt;
static uint8_t mix_opt_present;
static int32_t multi_a;
static int32_t multi_b;
static int32_t multi_c;
static uint32_t slow_ms = 30U;

/**
 * @brief 裸 AT
 * @param ctx 未使用
 * @return LW_AT_OK
 */
static at_rc_t hs_at_exe(void *ctx)
{
    (void)ctx;
    return LW_AT_OK;
}

/**
 * @brief AT+PING
 * @param ctx 未使用
 * @return 中间信息成败
 */
static at_rc_t hs_ping_exe(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+PING:PONG") >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+INT=?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t hs_int_test(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+INT:(int32)") >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+INT?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t hs_int_query(void *ctx)
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
static at_rc_t hs_int_setup(uint8_t para_num, void *ctx)
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
 * @brief AT+INT 执行清零
 * @param ctx 未使用
 * @return LW_AT_OK
 */
static at_rc_t hs_int_exe(void *ctx)
{
    (void)ctx;
    int_val = 0;
    return LW_AT_OK;
}

/**
 * @brief AT+STR=?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t hs_str_test(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+STR:(string)") >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+STR?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t hs_str_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+STR:%s", str_val) >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+STR=<str>
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t hs_str_setup(uint8_t para_num, void *ctx)
{
    const char *s;
    size_t n;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_str(0U, &s) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    n = strlen(s);
    if (n >= HS_CMD_STR_CAP) {
        return LW_AT_ERROR;
    }
    memcpy(str_val, s, n + 1U);
    return LW_AT_OK;
}

/**
 * @brief AT+STR 执行清空
 * @param ctx 未使用
 * @return LW_AT_OK
 */
static at_rc_t hs_str_exe(void *ctx)
{
    (void)ctx;
    str_val[0] = '\0';
    return LW_AT_OK;
}

/**
 * @brief AT+MIX=<int>,<str>[,<opt>]
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t hs_mix_setup(uint8_t para_num, void *ctx)
{
    const char *s;
    size_t n;
    int32_t a;
    int32_t opt;

    (void)ctx;
    if (para_num < 2U) {
        return LW_AT_ERROR;
    }
    if (lw_at_get_para_digit(0U, &a) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if (lw_at_get_para_str(1U, &s) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    n = strlen(s);
    if (n >= HS_CMD_STR_CAP) {
        return LW_AT_ERROR;
    }
    mix_a = a;
    memcpy(mix_str, s, n + 1U);
    mix_opt_present = 0U;
    mix_opt = 0;
    if (para_num >= 3U) {
        if (lw_at_get_para_digit(2U, &opt) == LW_AT_PARA_OK) {
            mix_opt = opt;
            mix_opt_present = 1U;
        } else if (lw_at_get_para_str(2U, &s) == LW_AT_PARA_OMITTED) {
            mix_opt_present = 0U;
        } else {
            return LW_AT_ERROR;
        }
    }
    return LW_AT_OK;
}

/**
 * @brief AT+MIX?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t hs_mix_query(void *ctx)
{
    (void)ctx;
    if (mix_opt_present != 0U) {
        return (lw_at_send_line("+MIX:%ld,%s,%ld", (long)mix_a, mix_str,
                                (long)mix_opt) >= 0)
                   ? LW_AT_OK
                   : LW_AT_ERROR;
    }
    return (lw_at_send_line("+MIX:%ld,%s", (long)mix_a, mix_str) >= 0)
               ? LW_AT_OK
               : LW_AT_ERROR;
}

/**
 * @brief AT+MULTI=<a>,<b>,<c>
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t hs_multi_setup(uint8_t para_num, void *ctx)
{
    int32_t a;
    int32_t b;
    int32_t c;

    (void)ctx;
    if (para_num != 3U) {
        return LW_AT_ERROR;
    }
    if ((lw_at_get_para_digit(0U, &a) != LW_AT_PARA_OK) ||
        (lw_at_get_para_digit(1U, &b) != LW_AT_PARA_OK) ||
        (lw_at_get_para_digit(2U, &c) != LW_AT_PARA_OK)) {
        return LW_AT_ERROR;
    }
    multi_a = a;
    multi_b = b;
    multi_c = c;
    return LW_AT_OK;
}

/**
 * @brief AT+FAIL 固定失败
 * @param ctx 未使用
 * @return LW_AT_ERROR
 */
static at_rc_t hs_fail_exe(void *ctx)
{
    (void)ctx;
    return LW_AT_ERROR;
}

/**
 * @brief AT+FAIL=<n> 固定失败
 * @param para_num 槽数
 * @param ctx      未使用
 * @return LW_AT_ERROR
 */
static at_rc_t hs_fail_setup(uint8_t para_num, void *ctx)
{
    (void)para_num;
    (void)ctx;
    return LW_AT_ERROR;
}

/**
 * @brief AT+SLOW 延时后 OK
 * @param ctx 未使用
 * @return LW_AT_OK
 */
static at_rc_t hs_slow_exe(void *ctx)
{
    (void)ctx;
    if (slow_ms > 0U) {
        Sleep(slow_ms);
    }
    return LW_AT_OK;
}

/* 应用侧 CIPMODE：0 普通；1 允许无参 CIPSEND 进流式 */
#define HS_CIPMODE_NORMAL 0
#define HS_CIPMODE_PASS   1

static int32_t cipmode;

/**
 * @brief AT+CIPMODE?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t hs_cipmode_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+CIPMODE:%ld", (long)cipmode) >= 0) ? LW_AT_OK
                                                                : LW_AT_ERROR;
}

/**
 * @brief AT+CIPMODE=<0|1>
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t hs_cipmode_setup(uint8_t para_num, void *ctx)
{
    int32_t mode;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &mode) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if ((mode != HS_CIPMODE_NORMAL) && (mode != HS_CIPMODE_PASS)) {
        return LW_AT_ERROR;
    }
    cipmode = mode;
    return LW_AT_OK;
}

/**
 * @brief AT+CIPSEND 无参：仅 CIPMODE=1 时进流式（handler 内同步 confirm）
 * @param ctx 未使用
 * @return LW_AT_OK 或 LW_AT_ERROR
 */
static at_rc_t hs_cipsend_exe(void *ctx)
{
    (void)ctx;
    if (cipmode != HS_CIPMODE_PASS) {
        return LW_AT_ERROR;
    }
    if (lw_at_transmit_enter() != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    (void)lw_at_data_confirm();
    return LW_AT_OK;
}

static const lw_at_cmd_t hs_basic_cmds[] = {
    { "",       NULL,        NULL,         NULL,          hs_at_exe     },
    { "+PING",  NULL,        NULL,         NULL,          hs_ping_exe   },
    { "+INT",   hs_int_test, hs_int_query, hs_int_setup,  hs_int_exe    },
    { "+STR",   hs_str_test, hs_str_query, hs_str_setup,  hs_str_exe    },
    { "+MIX",   NULL,        hs_mix_query, hs_mix_setup,  NULL          },
    { "+MULTI", NULL,        NULL,         hs_multi_setup, NULL         },
    { "+FAIL",  NULL,        NULL,         hs_fail_setup, hs_fail_exe   },
    { "+SLOW",  NULL,        NULL,         NULL,          hs_slow_exe   },
};

static lw_at_cmd_table_t hs_basic_table = {
    hs_basic_cmds,
    (uint16_t)(sizeof(hs_basic_cmds) / sizeof(hs_basic_cmds[0])),
    NULL,
};

static const lw_at_cmd_t hs_data_cmds[] = {
    { "+CIPMODE", NULL, hs_cipmode_query, hs_cipmode_setup, NULL },
    { "+CIPSEND", NULL, NULL, NULL, hs_cipsend_exe },
};

static lw_at_cmd_table_t hs_data_table = {
    hs_data_cmds,
    (uint16_t)(sizeof(hs_data_cmds) / sizeof(hs_data_cmds[0])),
    NULL,
};

void hs_cmd_reset_state(void)
{
    int_val = 0;
    str_val[0] = '\0';
    mix_a = 0;
    mix_str[0] = '\0';
    mix_opt = 0;
    mix_opt_present = 0U;
    multi_a = 0;
    multi_b = 0;
    multi_c = 0;
    cipmode = HS_CIPMODE_NORMAL;
}

int32_t hs_cmd_int_get(void)
{
    return int_val;
}

const char *hs_cmd_str_get(void)
{
    return str_val;
}

void hs_cmd_mix_get(int32_t *a, char *str, uint32_t str_cap, int32_t *opt,
                    uint8_t *opt_present)
{
    if (a != NULL) {
        *a = mix_a;
    }
    if ((str != NULL) && (str_cap > 0U)) {
        strncpy(str, mix_str, str_cap - 1U);
        str[str_cap - 1U] = '\0';
    }
    if (opt != NULL) {
        *opt = mix_opt;
    }
    if (opt_present != NULL) {
        *opt_present = mix_opt_present;
    }
}

void hs_cmd_multi_get(int32_t *a, int32_t *b, int32_t *c)
{
    if (a != NULL) {
        *a = multi_a;
    }
    if (b != NULL) {
        *b = multi_b;
    }
    if (c != NULL) {
        *c = multi_c;
    }
}

void hs_cmd_slow_set_ms(uint32_t ms)
{
    slow_ms = ms;
}

lw_at_err_t hs_cmd_register(void)
{
    lw_at_err_t err;

    hs_basic_table.next = NULL;
    hs_data_table.next = NULL;
    cipmode = HS_CIPMODE_NORMAL;
    err = lw_at_cmd_register(&hs_basic_table);
    if (err != LW_AT_ERR_OK) {
        return err;
    }
    return lw_at_cmd_register(&hs_data_table);
}
