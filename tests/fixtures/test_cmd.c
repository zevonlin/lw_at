/**
 * @file test_cmd.c
 * @brief 测试用命令表实现：AT、ECHO、可选 CIPMODE/CIPSEND
 *
 * @details
 * 基础表始终注册；数据模式表（CIPMODE 应用配置、CIPSEND 定长/流式）
 * 恒编译。对齐 ESP-AT 用法；命令名仅存在于测试夹具，非库内核。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 2.4.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                    version
 * 2026-08-01 linzhiwei 移除 LW_AT_CFG_TRANSMIT 条件编译           v2.4.0
 * 2026-08-01 linzhiwei CIPSEND 改为 handler 内同步 confirm        v2.3.0
 * 2026-07-31 linzhiwei TRANS/SEND 改为 CIPMODE+CIPSEND            v2.2.0
 * 2026-07-31 linzhiwei 增加 AT+SEND 定长收数夹具命令         v2.1.0
 * 2026-07-30 linzhiwei 由 cmd_demo 重命名为 test_cmd       v2.0.0
 * 2026-07-30 linzhiwei 迁入 tests/fixtures                 v1.3.0
 * 2026-07-30 linzhiwei 首次发布                            v1.0.0
 */
#include "test_cmd.h"

/* AT+ECHO 当前保存的整型值，供 query/setup/exe 共享 */
static int32_t echo_val;

/* 最近一次 AT+ECHO 设置命令收到的参数个数，供测试断言 */
static uint8_t echo_para_num;

/**
 * @brief 裸 AT 执行：联通性检查，直接成功
 * @param ctx 用户上下文（未使用）
 * @return LW_AT_OK
 */
static at_rc_t test_cmd_at_exe(void *ctx)
{
    (void)ctx;
    return LW_AT_OK;
}

/**
 * @brief AT+ECHO=? 测试：输出参数范围说明
 * @param ctx 用户上下文（未使用）
 * @return 中间信息发送成功返回 LW_AT_OK，否则 LW_AT_ERROR
 */
static at_rc_t test_cmd_echo_test(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+ECHO:(int32)") >= 0) ? LW_AT_OK : LW_AT_ERROR;
}

/**
 * @brief AT+ECHO? 查询：输出当前保存的值
 * @param ctx 用户上下文（未使用）
 * @return 中间信息发送成功返回 LW_AT_OK，否则 LW_AT_ERROR
 */
static at_rc_t test_cmd_echo_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+ECHO:%ld", (long)echo_val) >= 0) ? LW_AT_OK
                                                               : LW_AT_ERROR;
}

/**
 * @brief AT+ECHO=<val>[,...] 设置：首参数须为十进制整数，存入 echo_val
 * @param para_num 参数个数
 * @param ctx      用户上下文（未使用）
 * @return 首参数合法返回 LW_AT_OK，否则 LW_AT_ERROR
 */
static at_rc_t test_cmd_echo_setup(uint8_t para_num, void *ctx)
{
    /* 首槽解析出的整型值 */
    int32_t val;

    (void)ctx;
    if (lw_at_get_para_digit(0U, &val) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    echo_val = val;
    echo_para_num = para_num;
    return LW_AT_OK;
}

/**
 * @brief AT+ECHO 执行：复位保存值
 * @param ctx 用户上下文（未使用）
 * @return LW_AT_OK
 */
static at_rc_t test_cmd_echo_exe(void *ctx)
{
    (void)ctx;
    echo_val = 0;
    return LW_AT_OK;
}

/* 定长捕获缓冲容量（含 NUL） */
#define TEST_CMD_SEND_CAP 64U

/* 应用侧传输模式：0 普通；1 允许无参 CIPSEND 进流式 */
#define TEST_CMD_CIPMODE_NORMAL 0
#define TEST_CMD_CIPMODE_PASS   1

/* 定长窗口捕获缓冲与长度，供断言 */
static char send_buf[TEST_CMD_SEND_CAP];
static uint32_t send_len;
static uint32_t send_done_got;

/* 应用侧 CIPMODE，非内核状态 */
static int32_t cipmode;

/**
 * @brief AT+CIPMODE?
 * @param ctx 未使用
 * @return 成败
 */
static at_rc_t test_cmd_cipmode_query(void *ctx)
{
    (void)ctx;
    return (lw_at_send_line("+CIPMODE:%ld", (long)cipmode) >= 0) ? LW_AT_OK
                                                                : LW_AT_ERROR;
}

/**
 * @brief AT+CIPMODE=<0|1>：仅保存应用配置
 * @param para_num 槽数
 * @param ctx      未使用
 * @return 成败
 */
static at_rc_t test_cmd_cipmode_setup(uint8_t para_num, void *ctx)
{
    int32_t mode;

    (void)para_num;
    (void)ctx;
    if (lw_at_get_para_digit(0U, &mode) != LW_AT_PARA_OK) {
        return LW_AT_ERROR;
    }
    if ((mode != TEST_CMD_CIPMODE_NORMAL) && (mode != TEST_CMD_CIPMODE_PASS)) {
        return LW_AT_ERROR;
    }
    cipmode = mode;
    return LW_AT_OK;
}

/**
 * @brief 定长窗口分片：追加到 send_buf
 */
static void test_cmd_send_chunk(const uint8_t *data, uint32_t len, void *user)
{
    uint32_t i;

    (void)user;
    for (i = 0U; i < len; i++) {
        if (send_len + 1U >= TEST_CMD_SEND_CAP) {
            break;
        }
        send_buf[send_len] = (char)data[i];
        send_len++;
    }
    send_buf[send_len] = '\0';
}

/**
 * @brief 定长收满：记录 got（最终 OK 由库统一回）
 */
static void test_cmd_send_done(uint32_t got, void *user)
{
    (void)user;
    send_done_got = got;
}

/**
 * @brief AT+CIPSEND=<len>：定长收数（handler 内同步 confirm）
 * @param para_num 槽数
 * @param ctx      未使用
 * @return LW_AT_OK 或 LW_AT_ERROR
 */
static at_rc_t test_cmd_cipsend_setup(uint8_t para_num, void *ctx)
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
    send_len = 0U;
    send_buf[0] = '\0';
    send_done_got = 0U;
    lw_at_memset(&cfg, 0, sizeof(cfg));
    cfg.policy = LW_AT_DATA_FIXED;
    cfg.length = (uint32_t)len;
    cfg.on_chunk = test_cmd_send_chunk;
    cfg.on_done = test_cmd_send_done;
    if (lw_at_data_enter(&cfg) != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    /* 同步确认：> 由 core_exec_line 在 \r\nOK\r\n 后补打 */
    (void)lw_at_data_confirm();
    return LW_AT_OK;
}

/**
 * @brief AT+CIPSEND：无参；仅 CIPMODE=1 时进流式（handler 内同步 confirm）
 * @param ctx 未使用
 * @return LW_AT_OK 或 LW_AT_ERROR
 */
static at_rc_t test_cmd_cipsend_exe(void *ctx)
{
    (void)ctx;
    if (cipmode != TEST_CMD_CIPMODE_PASS) {
        return LW_AT_ERROR;
    }
    if (lw_at_transmit_enter() != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    (void)lw_at_data_confirm();
    return LW_AT_OK;
}

/* 基础命令数组：联通性检查 + ECHO 四形态 */
static const lw_at_cmd_t test_cmd_basic_cmds[] = {
    { "",      NULL,               NULL,                NULL,                test_cmd_at_exe   },
    { "+ECHO", test_cmd_echo_test, test_cmd_echo_query, test_cmd_echo_setup, test_cmd_echo_exe },
};

/* 基础命令表链表节点（用户静态提供，注册时由库改写 next） */
static lw_at_cmd_table_t test_cmd_basic_node = {
    test_cmd_basic_cmds,
    (uint16_t)(sizeof(test_cmd_basic_cmds) / sizeof(test_cmd_basic_cmds[0])),
    NULL,
};

/* 数据模式命令：CIPMODE + CIPSEND */
static const lw_at_cmd_t test_cmd_data_cmds[] = {
    { "+CIPMODE", NULL, test_cmd_cipmode_query, test_cmd_cipmode_setup, NULL },
    { "+CIPSEND", NULL, NULL, test_cmd_cipsend_setup, test_cmd_cipsend_exe },
};

/* 数据模式命令表链表节点 */
static lw_at_cmd_table_t test_cmd_data_node = {
    test_cmd_data_cmds,
    (uint16_t)(sizeof(test_cmd_data_cmds) / sizeof(test_cmd_data_cmds[0])),
    NULL,
};

/**
 * @brief 将测试命令表全部注册到字典
 */
lw_at_err_t test_cmd_register(void)
{
    /* 分表注册时保留首次失败码，便于调用方定位 */
    lw_at_err_t err;

    /* deinit/init 后节点 next 可能残留旧值，注册前清零 */
    test_cmd_basic_node.next = NULL;
    test_cmd_data_node.next = NULL;
    /* 每轮注册复位应用侧模式，避免用例间串扰 */
    cipmode = TEST_CMD_CIPMODE_NORMAL;

    err = lw_at_cmd_register(&test_cmd_basic_node);
    if (err != LW_AT_ERR_OK) {
        return err;
    }
    return lw_at_cmd_register(&test_cmd_data_node);
}

/**
 * @brief 取基础命令表节点
 */
lw_at_cmd_table_t *test_cmd_basic_table(void)
{
    return &test_cmd_basic_node;
}

/**
 * @brief 取数据模式命令表节点
 */
lw_at_cmd_table_t *test_cmd_trans_table(void)
{
    return &test_cmd_data_node;
}

/**
 * @brief 取定长 CIPSEND 已捕获正文
 */
const char *test_cmd_send_buf_get(void)
{
    return send_buf;
}

/**
 * @brief 取定长 CIPSEND on_done 收到的 got
 */
uint32_t test_cmd_send_done_got(void)
{
    return send_done_got;
}

/**
 * @brief 取 AT+ECHO 当前保存的值
 */
int32_t test_cmd_echo_get(void)
{
    return echo_val;
}

/**
 * @brief 取最近一次 AT+ECHO 设置命令的参数个数
 */
uint8_t test_cmd_para_num_get(void)
{
    return echo_para_num;
}
