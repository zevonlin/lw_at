/**
 * @file send_cmd.c
 * @brief host_slave_send 专题命令实现
 *
 * @details
 * AT / CIPMODE / CIPABORT / CIPSEND（定长+流式）。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 1.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei CIPSEND 改为 handler 内同步 confirm          v1.1.0
 * 2026-07-31 linzhiwei 首次发布                                    v1.0.0
 */
#include "send_cmd.h"

#include <string.h>

#define SEND_CMD_FIXED_CAP 256U
#define SEND_CMD_CIPMODE_NORMAL 0
#define SEND_CMD_CIPMODE_PASS 1

static int32_t cipmode;
static int32_t cipabort;
static char fixed_buf[SEND_CMD_FIXED_CAP];
static uint32_t fixed_len;
static uint32_t fixed_done_got;

/**
 * @brief AT 联通
 * @param ctx 未使用
 * @return LW_AT_OK
 */
static at_rc_t send_cmd_at_exe(void *ctx)
{
    (void)ctx;
    return LW_AT_OK;
}

/**
 * @brief AT+CIPMODE?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t send_cmd_cipmode_query(void *ctx)
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
static at_rc_t send_cmd_cipmode_setup(uint8_t para_num, void *ctx)
{
    int32_t mode;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &mode) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if ((mode != SEND_CMD_CIPMODE_NORMAL) && (mode != SEND_CMD_CIPMODE_PASS)) {
        return LW_AT_ERROR;
    }
    cipmode = mode;
    return LW_AT_OK;
}

/**
 * @brief AT+CIPABORT?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t send_cmd_cipabort_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+CIPABORT:%ld", (long)cipabort) >= 0) ? LW_AT_OK
                                                                  : LW_AT_ERROR;
}

/**
 * @brief AT+CIPABORT=<0|1>：后续定长是否允许 +++
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t send_cmd_cipabort_setup(uint8_t para_num, void *ctx)
{
    int32_t mode;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &mode) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if ((mode != 0) && (mode != 1)) {
        return LW_AT_ERROR;
    }
    cipabort = mode;
    return LW_AT_OK;
}

/**
 * @brief 定长分片回调
 */
static void send_cmd_fixed_chunk(const uint8_t *data, uint32_t len, void *user)
{
    uint32_t i;

    (void)user;
    for (i = 0U; i < len; i++) {
        if (fixed_len + 1U >= SEND_CMD_FIXED_CAP) {
            break;
        }
        fixed_buf[fixed_len] = (char)data[i];
        fixed_len++;
    }
    fixed_buf[fixed_len] = '\0';
}

/**
 * @brief 定长完成/中止回调
 */
static void send_cmd_fixed_done(uint32_t got, void *user)
{
    (void)user;
    fixed_done_got = got;
}

/**
 * @brief AT+CIPSEND=<len> 定长（handler 内同步 confirm）
 * @param para_num 槽数
 * @param ctx      未使用
 * @return LW_AT_OK 或 LW_AT_ERROR
 */
static at_rc_t send_cmd_cipsend_setup(uint8_t para_num, void *ctx)
{
    int32_t len;
    lw_at_data_enter_cfg_t cfg;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &len) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if (len <= 0) {
        return LW_AT_ERROR;
    }
    fixed_len = 0U;
    fixed_buf[0] = '\0';
    fixed_done_got = 0U;
    lw_at_memset(&cfg, 0, sizeof(cfg));
    cfg.policy = LW_AT_DATA_FIXED;
    cfg.length = (uint32_t)len;
    cfg.allow_plus_abort = (uint8_t)((cipabort != 0) ? 1U : 0U);
    cfg.on_chunk = send_cmd_fixed_chunk;
    cfg.on_done = send_cmd_fixed_done;
    if (lw_at_data_enter(&cfg) != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    (void)lw_at_data_confirm();
    return LW_AT_OK;
}

/**
 * @brief AT+CIPSEND 无参流式（handler 内同步 confirm）
 * @param ctx 未使用
 * @return LW_AT_OK 或 LW_AT_ERROR
 */
static at_rc_t send_cmd_cipsend_exe(void *ctx)
{
    (void)ctx;
    if (cipmode != SEND_CMD_CIPMODE_PASS) {
        return LW_AT_ERROR;
    }
    if (lw_at_transmit_enter() != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    (void)lw_at_data_confirm();
    return LW_AT_OK;
}

static const lw_at_cmd_t send_cmd_table_cmds[] = {
    { "",          NULL, NULL,                   NULL,                    send_cmd_at_exe        },
    { "+CIPMODE",  NULL, send_cmd_cipmode_query, send_cmd_cipmode_setup,  NULL                   },
    { "+CIPABORT", NULL, send_cmd_cipabort_query, send_cmd_cipabort_setup, NULL                  },
    { "+CIPSEND",  NULL, NULL,                   send_cmd_cipsend_setup,  send_cmd_cipsend_exe   },
};

static lw_at_cmd_table_t send_cmd_table = {
    send_cmd_table_cmds,
    (uint16_t)(sizeof(send_cmd_table_cmds) / sizeof(send_cmd_table_cmds[0])),
    NULL,
};

lw_at_err_t send_cmd_register(void)
{
    return lw_at_cmd_register(&send_cmd_table);
}

void send_cmd_reset_state(void)
{
    cipmode = SEND_CMD_CIPMODE_NORMAL;
    cipabort = 0;
    fixed_len = 0U;
    fixed_buf[0] = '\0';
    fixed_done_got = 0U;
}

const char *send_cmd_fixed_buf_get(void)
{
    return fixed_buf;
}

uint32_t send_cmd_fixed_len_get(void)
{
    return fixed_len;
}

uint32_t send_cmd_fixed_done_got(void)
{
    return fixed_done_got;
}

int32_t send_cmd_cipmode_get(void)
{
    return cipmode;
}

int32_t send_cmd_cipabort_get(void)
{
    return cipabort;
}
