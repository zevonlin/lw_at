/**
 * @file test_main.c
 * @brief lw_at_core 模块黑盒集成测试
 *
 * @details
 * 基于 PC 适配层对 core 做主机↔从机交互验证。
 * 覆盖 init/注册、四种命令形态、错误行、分片、空闲门控、缓存满、行超长、
 * 设置拆槽字符矩阵（C12）、透传进入/转发/+++ 守卫、定长 SEND（C14）。
 * 详见同目录 TEST.md。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-01
 * @version 1.5.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-01 linzhiwei 移除 LW_AT_CFG_TRANSMIT 条件编译             v1.5.0
 * 2026-08-01 linzhiwei 事件驱动适配：虚拟定时器；退出由 process 完成 v1.4.0
 * 2026-07-31 linzhiwei 增加定长数据窗口 C14                   v1.3.0
 * 2026-07-31 linzhiwei 增加空 feed / process 退出等加固用例   v1.2.0
 * 2026-07-30 linzhiwei 扩展 C12 设置拆槽字符矩阵             v1.1.0
 * 2026-07-30 linzhiwei 自宿主机例程迁入本目录并编号 C01–C11 v1.0.0
 */
#include <stdio.h>
#include <string.h>

#include "lw_at.h"
#include "test_cmd.h"
#include "test_port.h"

/* 常规用例的缓冲配置 */
#define TEST_RX_BUF_SIZE 128U
#define TEST_LINE_BUF_SIZE 64U
#define TEST_TX_BUF_SIZE 64U

/* 空闲阈值与 +++ 静默守卫时长（ms） */
#define TEST_IDLE_MS 50U

/* 不足守卫/空闲阈值的短间隔（ms） */
#define TEST_SHORT_MS 1U
#define TEST_GUARD_MS 100U

/* 缓存满用例：小接收缓存与超量数据长度 */
#define TEST_SMALL_RX_SIZE 16U
#define TEST_OVERFLOW_FEED_LEN 32U

/* 行超长用例的小组装区 */
#define TEST_SMALL_LINE_SIZE 8U

/* 透传 sink 捕获缓冲字节数 */
#define TEST_SINK_BUF_SIZE 256U

/* 转义打印临时缓冲字节数 */
#define TEST_ESC_BUF_SIZE 512U

/* AT+ECHO 用例的期望值 */
#define TEST_ECHO_VAL 42
#define TEST_ECHO_NEG_VAL (-7)
#define TEST_ECHO_PARA_NUM 3U

/* 最终结果行的确切字节形式 */
#define TEST_RSP_OK "\r\nOK\r\n"
#define TEST_RSP_ERROR "\r\nERROR\r\n"
#define TEST_RSP_OK_PROMPT "\r\nOK\r\n>\r\n"

/* 库接收环形缓存存储区 */
static uint8_t rx_mem[TEST_RX_BUF_SIZE];

/* 单行命令组装区 */
static uint8_t line_mem[TEST_LINE_BUF_SIZE];

/* lw_at_send_line 格式化区 */
static uint8_t tx_mem[TEST_TX_BUF_SIZE];

/* 当前用例使用的库配置副本 */
static lw_at_config_t test_cfg;

/* 透传 sink 捕获的下行数据（NUL 结尾） */
static char sink_buf[TEST_SINK_BUF_SIZE];

/* sink 捕获缓冲当前长度 */
static uint32_t sink_len;

/* 空闲通知回调累计触发次数 */
static uint32_t idle_cnt;

/* 断言总数 */
static uint32_t check_total;

/* 断言失败数 */
static uint32_t check_fail;

/**
 * @brief 将字符串转义为可见形式（\r \n 等），写入静态缓冲
 * @param s 原始字符串，可为 NULL
 * @return 转义后的静态缓冲（下次调用会被覆盖）
 */
static const char *test_esc(const char *s)
{
    /* 转义输出缓冲：仅供打印，下次调用覆盖 */
    static char esc_buf[TEST_ESC_BUF_SIZE];
    /* 写入 esc_buf 的当前位置 */
    uint32_t i = 0U;
    /* 扫描原始字符串的游标 */
    const char *p;

    if (s == NULL) {
        esc_buf[0] = '\0';
        return esc_buf;
    }
    for (p = s; (*p != '\0') && (i < (TEST_ESC_BUF_SIZE - 5U)); p++) {
        if (*p == '\r') {
            esc_buf[i++] = '\\';
            esc_buf[i++] = 'r';
        } else if (*p == '\n') {
            esc_buf[i++] = '\\';
            esc_buf[i++] = 'n';
        } else if (*p == '\t') {
            esc_buf[i++] = '\\';
            esc_buf[i++] = 't';
        } else if (*p == '\\') {
            esc_buf[i++] = '\\';
            esc_buf[i++] = '\\';
        } else if ((unsigned char)*p < 0x20U) {
            (void)snprintf(&esc_buf[i], TEST_ESC_BUF_SIZE - i, "\\x%02X",
                           (unsigned char)*p);
            i += 4U;
        } else {
            esc_buf[i++] = *p;
        }
    }
    esc_buf[i] = '\0';
    return esc_buf;
}

/**
 * @brief 打印用例横幅
 * @param title 用例标题
 */
static void test_banner(const char *title)
{
    (void)printf("\n==== %s ====\n", title);
}

/**
 * @brief 打印信息行（非断言）
 * @param msg 说明文字
 */
static void test_info(const char *msg)
{
    (void)printf("-- %s\n", msg);
}

/**
 * @brief 断言核心：登记结果并打印 PASS/FAIL
 * @param ok   非 0 表示通过
 * @param expr 断言表达式或说明
 * @param line 源码行号
 */
static void test_check(int ok, const char *expr, int line)
{
    check_total++;
    if (ok != 0) {
        (void)printf("[PASS] %s\n", expr);
    } else {
        check_fail++;
        (void)printf("[FAIL] line %d: %s\n", line, expr);
    }
}

#define TEST_CHECK(cond) test_check((cond) ? 1 : 0, #cond, __LINE__)

/**
 * @brief 透传 sink：把下行数据追加到捕获缓冲
 * @param data 数据起始地址
 * @param len  数据长度
 * @param user 用户指针（未使用）
 */
static void test_sink(const uint8_t *data, uint32_t len, void *user)
{
    uint32_t i;

    (void)user;
    for (i = 0U; i < len; i++) {
        if (sink_len < (TEST_SINK_BUF_SIZE - 1U)) {
            sink_buf[sink_len] = (char)data[i];
            sink_len++;
        }
    }
    sink_buf[sink_len] = '\0';
}

/**
 * @brief 空闲通知回调：计数
 * @param user 用户指针（未使用）
 */
static void test_idle_cb(void *user)
{
    (void)user;
    idle_cnt++;
}

/**
 * @brief 以指定缓冲尺寸重建库实例，并注册示例命令表
 * @param rx_size   接收缓存字节数
 * @param line_size 行组装区字节数
 * @param with_sink 非 0 时注册透传 sink
 */
static void test_setup(uint32_t rx_size, uint32_t line_size, uint8_t with_sink)
{
    lw_at_deinit();
    test_port_out_clear();
    sink_len = 0U;
    sink_buf[0] = '\0';
    idle_cnt = 0U;

    memset(&test_cfg, 0, sizeof(test_cfg));
    test_cfg.rx_buf = rx_mem;
    test_cfg.rx_buf_size = rx_size;
    test_cfg.line_buf = line_mem;
    test_cfg.line_buf_size = line_size;
    test_cfg.tx_buf = tx_mem;
    test_cfg.tx_buf_size = TEST_TX_BUF_SIZE;
    test_cfg.port.write = test_port_write;
    test_cfg.port.timer_arm = test_port_timer_arm;
    test_cfg.port.timer_stop = test_port_timer_stop;
    test_cfg.idle_timeout_ms = TEST_IDLE_MS;
    test_cfg.guard_ms = TEST_GUARD_MS;
    test_cfg.cbs.idle_cb = test_idle_cb;
    if (with_sink != 0U) {
        test_cfg.cbs.sink = test_sink;
    }
    TEST_CHECK(lw_at_init(&test_cfg) == LW_AT_ERR_OK);
    TEST_CHECK(test_cmd_register() == LW_AT_ERR_OK);
}

/**
 * @brief 喂入一个字符串（不含隐式结束符）
 * @param s 待喂入字符串
 * @return lw_at_feed 的返回值
 */
static int32_t test_feed(const char *s)
{
    return lw_at_feed((const uint8_t *)s, (uint32_t)strlen(s));
}

/**
 * @brief 推进到空闲（定时器到期触发回调置 pending）并驱动一轮 process
 */
static void test_idle_run(void)
{
    test_port_tick_advance(TEST_IDLE_MS);
    lw_at_process();
}

/**
 * @brief 发送一条命令并取回从机应答
 * @param s 命令字符串（须含 \r\n）
 * @return 本条命令产生的全部应答字节（NUL 结尾）
 */
static const char *test_send(const char *s)
{
    test_port_out_clear();
    (void)printf(">> \"%s\"\n", test_esc(s));
    (void)test_feed(s);
    test_idle_run();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    return test_port_out_get();
}

/**
 * @brief 应答断言：发送命令并逐字节比对期望应答
 * @param cmd    命令字符串
 * @param expect 期望应答的确切字节
 * @param line   源码行号
 */
static void test_resp(const char *cmd, const char *expect, int line)
{
    /* 本条命令实际收到的从机应答（捕获缓冲） */
    const char *got = test_send(cmd);

    check_total++;
    if (strcmp(got, expect) == 0) {
        (void)printf("[PASS] 应答匹配\n");
    } else {
        check_fail++;
        (void)printf("[FAIL] line %d: expect \"%s\"\n", line, test_esc(expect));
    }
}

#define TEST_RESP(cmd, expect) test_resp(cmd, expect, __LINE__)

/**
 * @brief 按 ESP 用法进入流式：CIPMODE=1 再无参 CIPSEND
 */
static void test_enter_stream(void)
{
    TEST_RESP("AT+CIPMODE=1\r\n", TEST_RSP_OK);
    TEST_RESP("AT+CIPSEND\r\n", TEST_RSP_OK_PROMPT);
}

/**
 * @brief 用例：初始化参数校验、命令表链表注册与重名拒绝
 */
static void test_case_init(void)
{
    /* 表内命令名重复的非法表 */
    static const lw_at_cmd_t dup_cmds[] = {
        { "+A", NULL, NULL, NULL, NULL },
        { "+A", NULL, NULL, NULL, NULL },
    };
    static lw_at_cmd_table_t dup_table = { dup_cmds, 2U, NULL };

    /* 与基础表示例冲突的跨表重名节点（+ECHO） */
    static const lw_at_cmd_t clash_cmds[] = {
        { "+ECHO", NULL, NULL, NULL, NULL },
    };
    static lw_at_cmd_table_t clash_table = { clash_cmds, 1U, NULL };

    test_banner("C01/C02 初始化与命令表链表注册");

    lw_at_deinit();
    TEST_CHECK(lw_at_init(NULL) == LW_AT_ERR_PARAM);
    TEST_CHECK(lw_at_feed((const uint8_t *)"A", 1U) == (int32_t)LW_AT_ERR_STATE);
    TEST_CHECK(lw_at_cmd_register(test_cmd_basic_table()) == LW_AT_ERR_STATE);

    test_info("正常 init + 分表注册");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 0U);
    TEST_CHECK(lw_at_init(&test_cfg) == LW_AT_ERR_STATE);

    test_info("表内重名应拒绝");
    dup_table.next = NULL;
    TEST_CHECK(lw_at_cmd_register(&dup_table) == LW_AT_ERR_PARAM);

    test_info("跨表重名应拒绝");
    clash_table.next = NULL;
    TEST_CHECK(lw_at_cmd_register(&clash_table) == LW_AT_ERR_PARAM);

    test_info("同一节点重复注册应拒绝");
    TEST_CHECK(lw_at_cmd_register(test_cmd_basic_table()) == LW_AT_ERR_STATE);
}

/**
 * @brief 用例：四种命令形态与参数传递
 */
static void test_case_forms(void)
{
    test_banner("C03 四种命令形态");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 0U);
    TEST_RESP("AT\r\n", TEST_RSP_OK);
    TEST_RESP("AT+ECHO=42\r\n", TEST_RSP_OK);
    TEST_CHECK(test_cmd_echo_get() == TEST_ECHO_VAL);
    TEST_CHECK(test_cmd_para_num_get() == 1U);
    TEST_RESP("AT+ECHO?\r\n", "\r\n+ECHO:42\r\n" TEST_RSP_OK);
    TEST_RESP("AT+ECHO=?\r\n", "\r\n+ECHO:(int32)\r\n" TEST_RSP_OK);
    TEST_RESP("AT+ECHO\r\n", TEST_RSP_OK);
    TEST_CHECK(test_cmd_echo_get() == 0);
    TEST_RESP("AT+ECHO=-7,x,y\r\n", TEST_RSP_OK);
    TEST_CHECK(test_cmd_echo_get() == TEST_ECHO_NEG_VAL);
    TEST_CHECK(test_cmd_para_num_get() == TEST_ECHO_PARA_NUM);
}

/**
 * @brief 用例：非法行与缺失 handler 一律回 ERROR，空行忽略
 */
static void test_case_errors(void)
{
    test_banner("C04 错误行与空行");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 0U);
    TEST_RESP("at\r\n", TEST_RSP_ERROR);
    TEST_RESP("XYZ\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+FOO\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+trans\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+ECHO?x\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+ECHO=\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+ECHO=abc\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+ECHO=1,2,3,4,5,6,7,8,9\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+CIPSEND?\r\n", TEST_RSP_ERROR);
    TEST_RESP("\r\n", "");
    TEST_RESP("\r\nAT\r\n", TEST_RSP_OK);
}

/**
 * @brief 用例：任意分片喂数与单次 process 处理多行
 */
static void test_case_fragment(void)
{
    test_banner("C05 分片喂数与多行处理");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 0U);

    test_port_out_clear();
    test_info("分片: AT+EC | HO=5\\r | \\n");
    (void)printf(">> 分片喂入 AT+ECHO=5\\r\\n\n");
    (void)test_feed("AT+EC");
    (void)test_feed("HO=5\r");
    (void)test_feed("\n");
    test_idle_run();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);

    TEST_RESP("AT\r\nAT+ECHO?\r\n", TEST_RSP_OK "\r\n+ECHO:5\r\n" TEST_RSP_OK);
}

/**
 * @brief 用例：待处理标志门控与空闲通知只触发一次
 */
static void test_case_idle(void)
{
    test_banner("C06 空闲定时与待处理标志");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 0U);

    test_port_out_clear();
    (void)printf(">> \"%s\"（空闲未到期）\n", test_esc("AT\r\n"));
    (void)test_feed("AT\r\n");
    lw_at_process();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(test_port_out_get()[0] == '\0');
    TEST_CHECK(idle_cnt == 0U);

    test_info("推进空闲阈值，定时器到期触发");
    test_port_tick_advance(TEST_IDLE_MS);
    TEST_CHECK(idle_cnt == 1U);
    /* 单次定时器：到期后不再重复触发 */
    test_port_tick_advance(TEST_IDLE_MS);
    TEST_CHECK(idle_cnt == 1U);
    lw_at_process();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);

    (void)test_feed("AT\r\n");
    test_port_tick_advance(TEST_IDLE_MS);
    TEST_CHECK(idle_cnt == 2U);
    lw_at_process();

    test_info("空 feed(len=0) 不得重载定时器，否则命令会饿死");
    test_port_out_clear();
    idle_cnt = 0U;
    (void)test_feed("AT\r\n");
    test_port_tick_advance(TEST_IDLE_MS - TEST_SHORT_MS);
    TEST_CHECK(lw_at_feed(NULL, 0U) == 0);
    TEST_CHECK(lw_at_feed((const uint8_t *)"", 0U) == 0);
    test_port_tick_advance(TEST_SHORT_MS + 1U);
    lw_at_process();
    (void)printf("<< \"%s\" idle_cnt=%u\n", test_esc(test_port_out_get()),
                 (unsigned)idle_cnt);
    TEST_CHECK(idle_cnt == 1U);
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);
}

/**
 * @brief 用例：缓存满丢弃新数据，处理时回 ERROR 并清空整段缓存
 */
static void test_case_overflow(void)
{
    /* 超量垃圾数据，用于触发接收环形缓存满 */
    char junk[TEST_OVERFLOW_FEED_LEN];

    test_banner("C07 接收缓存满");
    memset(junk, (int)'X', sizeof(junk));
    test_setup(TEST_SMALL_RX_SIZE, TEST_LINE_BUF_SIZE, 0U);

    test_port_out_clear();
    test_info("喂入超量数据，期望只写入 rx_size-1");
    TEST_CHECK(lw_at_feed((const uint8_t *)junk, TEST_OVERFLOW_FEED_LEN) ==
               (int32_t)(TEST_SMALL_RX_SIZE - 1U));
    test_idle_run();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_ERROR) == 0);
    TEST_RESP("AT\r\n", TEST_RSP_OK);

    test_port_out_clear();
    test_info("已有完整行后再溢出，仍整段丢弃");
    (void)test_feed("AT\r\n");
    (void)lw_at_feed((const uint8_t *)junk, TEST_OVERFLOW_FEED_LEN);
    test_idle_run();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_ERROR) == 0);
    TEST_RESP("AT\r\n", TEST_RSP_OK);
}

/**
 * @brief 用例：行长超出组装区回 ERROR 并丢弃该行
 */
static void test_case_line_long(void)
{
    test_banner("C08 行超长");
    test_setup(TEST_RX_BUF_SIZE, TEST_SMALL_LINE_SIZE, 0U);
    TEST_RESP("AT+ECHO=1234\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT\r\n", TEST_RSP_OK);
}

/**
 * @brief 用例：透传进入、半行作废、下行转发与正常退出
 */
static void test_case_transmit_basic(void)
{
    test_banner("C09/C10 透传进入/转发/退出");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 1U);

    TEST_RESP("AT+CIPMODE=1\r\n", TEST_RSP_OK);
    TEST_RESP("AT+CIPSEND\r\nAT+EC", TEST_RSP_OK_PROMPT);
    test_info("已进入透传，半行 AT+EC 已作废");

    (void)printf(">> \"%s\"（透传数据）\n", test_esc("hello"));
    (void)test_feed("hello");
    lw_at_process();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "hello") == 0);

    (void)printf(">> \"%s\"（透传下不当 AT 解析）\n", test_esc("AT\r\n"));
    (void)test_feed("AT\r\n");
    lw_at_process();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "helloAT\r\n") == 0);

    test_port_out_clear();
    test_info("静默 + +++ + 静默，经定时器回调 + process 退出，默认不回 OK");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    test_port_tick_advance(TEST_GUARD_MS);
    lw_at_process();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(test_port_out_get()[0] == '\0');
    TEST_CHECK(strcmp(sink_buf, "helloAT\r\n") == 0);
    TEST_RESP("AT\r\n", TEST_RSP_OK);

    test_info("未注册 sink 时进入透传应失败");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 0U);
    TEST_RESP("AT+CIPMODE=1\r\n", TEST_RSP_OK);
    TEST_RESP("AT+CIPSEND\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT\r\n", TEST_RSP_OK);
}

/**
 * @brief 用例：+++ 守卫的各失败路径与 feed 路径退出
 */
static void test_case_transmit_guard(void)
{
    test_banner("C11 透传 +++ 守卫");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 1U);
    test_enter_stream();

    (void)test_feed("data");
    lw_at_process();
    sink_len = 0U;
    sink_buf[0] = '\0';
    test_info("前静默不足：+++ 应按普通数据转发");
    test_port_tick_advance(TEST_SHORT_MS);
    (void)printf(">> \"+++\"\n");
    (void)test_feed("+++");
    lw_at_process();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "+++") == 0);

    sink_len = 0U;
    sink_buf[0] = '\0';
    test_info("后静默不足：+++ 连同后续字节还原转发");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    test_port_tick_advance(TEST_SHORT_MS);
    (void)test_feed("z");
    lw_at_process();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "+++z") == 0);

    sink_len = 0U;
    sink_buf[0] = '\0';
    test_info("序列破坏 ++x");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("++x");
    lw_at_process();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "++x") == 0);

    sink_len = 0U;
    sink_buf[0] = '\0';
    test_info("第四个 +：整段还原");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("++++");
    lw_at_process();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "++++") == 0);

    test_info("feed 路径退出：静默后新命令按命令模式解析");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    test_port_tick_advance(TEST_GUARD_MS);
    lw_at_process();
    TEST_RESP("AT\r\n", TEST_RSP_OK);

    test_info("feed 退出前未 process 的透传残留须先交 sink，不得当 AT");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 1U);
    test_enter_stream();
    (void)test_feed("RAW");
    /* 故意不 process，使 ring 残留透传数据 */
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    test_port_tick_advance(TEST_GUARD_MS);
    sink_len = 0U;
    sink_buf[0] = '\0';
    test_port_out_clear();
    /* 先 process 完成退出排空，再发命令走命令模式 */
    lw_at_process();
    (void)test_feed("AT\r\n");
    test_idle_run();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    (void)printf("out \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(sink_buf, "RAW") == 0);
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);
}

/**
 * @brief 用例：透传退出加固（空 feed 不退出；process 可完成待退出）
 */
static void test_case_transmit_exit_harden(void)
{
    test_banner("C13 透传退出加固");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 1U);
    test_enter_stream();

    test_info("凑齐 +++ 后空 feed 不得确认退出；同刻再喂数据应还原 +++");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    /* 不推进时间：后静默未满足；空 feed 也不得 touch */
    TEST_CHECK(lw_at_feed(NULL, 0U) == 0);
    lw_at_process();
    sink_len = 0U;
    sink_buf[0] = '\0';
    (void)test_feed("keep");
    lw_at_process();
    (void)printf("sink \"%s\"\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "+++keep") == 0);

    test_info("定时器回调路径正常退出后再进透传");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    test_port_tick_advance(TEST_GUARD_MS);
    lw_at_process();
    TEST_RESP("AT\r\n", TEST_RSP_OK);

    test_info("feed 置 exit_req 后仅 process 也应排空并切回命令模式");
    test_enter_stream();
    (void)test_feed("BUF");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    test_port_tick_advance(TEST_GUARD_MS);
    sink_len = 0U;
    sink_buf[0] = '\0';
    test_port_out_clear();
    /* 仅 process 完成退出排空（feed 置 exit_req 后不调 tick） */
    lw_at_process();
    (void)printf("sink \"%s\"（process leave）\n", test_esc(sink_buf));
    TEST_CHECK(strcmp(sink_buf, "BUF") == 0);
    TEST_CHECK(test_port_out_get()[0] == '\0');
    /* 退出后发命令走命令模式 */
    (void)test_feed("AT\r\n");
    test_idle_run();
    (void)printf("out \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);

    /*
     * 回归：confirm 后不发任何数据，静默到期直接发 +++ 应能退出。
     * 锁定「lw_at_data_confirm 须用 guard_ms 重载定时器」这一契约；
     * 若 confirm 未 arm guard 定时器，silent 恒为 0，+++ 会被当普通数据，
     * 本用例即失败（sink 收到 "+++" 而无法退出）。
     */
    test_info("confirm 后无数据，静默到期直接 +++ 应退出");
    test_enter_stream();
    sink_len = 0U;
    sink_buf[0] = '\0';
    test_port_out_clear();
    /* 关键：进入 DATA 后不喂任何字节，仅推进 guard 时长触发定时器 */
    test_port_tick_advance(TEST_GUARD_MS);
    TEST_CHECK(test_port_out_get()[0] == '\0');
    (void)test_feed("+++");
    test_port_tick_advance(TEST_GUARD_MS);
    lw_at_process();
    (void)printf("sink \"%s\"（应为空）out \"%s\"\n", test_esc(sink_buf),
                 test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(sink_buf, "") == 0);
    TEST_CHECK(test_port_out_get()[0] == '\0');
    /* 退出后命令模式恢复 */
    TEST_RESP("AT\r\n", TEST_RSP_OK);
}

/**
 * @brief 用例：定长数据窗口（进入提示 + 收满后普通 OK）
 */
static void test_case_data_fixed(void)
{
    test_banner("C14 定长数据窗口（类 CIPSEND）");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 1U);

    test_info("AT+CIPSEND=5 应回 OK 与 '>'，再收满 5 字节后普通 OK");
    TEST_RESP("AT+CIPSEND=5\r\n", TEST_RSP_OK_PROMPT);
    test_port_out_clear();
    (void)test_feed("ABC");
    lw_at_process();
    TEST_CHECK(test_port_out_get()[0] == '\0');
    TEST_CHECK(strcmp(test_cmd_send_buf_get(), "ABC") == 0);
    (void)test_feed("DE");
    lw_at_process();
    (void)printf("out \"%s\" send \"%s\" got=%u\n", test_esc(test_port_out_get()),
                 test_esc(test_cmd_send_buf_get()),
                 (unsigned)test_cmd_send_done_got());
    TEST_CHECK(strcmp(test_cmd_send_buf_get(), "ABCDE") == 0);
    TEST_CHECK(test_cmd_send_done_got() == 5U);
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);

    test_info("收满后应已回命令模式");
    TEST_RESP("AT\r\n", TEST_RSP_OK);

    test_info("非法长度与分片喂满");
    TEST_RESP("AT+CIPSEND=0\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT+CIPSEND=4\r\n", TEST_RSP_OK_PROMPT);
    test_port_out_clear();
    (void)test_feed("12");
    lw_at_process();
    (void)test_feed("34");
    lw_at_process();
    TEST_CHECK(strcmp(test_cmd_send_buf_get(), "1234") == 0);
    TEST_CHECK(test_cmd_send_done_got() == 4U);
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);
}

/* 槽捕获：供设置拆槽测例在 setup 内保存各槽 */
#define TEST_SLOT_CAP 8U
#define TEST_SLOT_TEXT 64U

static char slot_text[TEST_SLOT_CAP][TEST_SLOT_TEXT];
static lw_at_para_rc_t slot_str_rc[TEST_SLOT_CAP];
static lw_at_para_rc_t slot_digit_rc[TEST_SLOT_CAP];
static int32_t slot_digit_val[TEST_SLOT_CAP];
static uint8_t slot_para_num;
/* setup 被调用次数：拆槽失败时不应增加 */
static uint32_t slot_setup_calls;

/**
 * @brief 测试用 setup：捕获各槽 str/digit 取参结果
 * @param para_num 槽个数
 * @param ctx      未使用
 * @return LW_AT_OK
 */
static at_rc_t test_slot_setup(uint8_t para_num, void *ctx)
{
    uint8_t i;

    (void)ctx;
    slot_setup_calls++;
    slot_para_num = para_num;
    for (i = 0U; i < TEST_SLOT_CAP; i++) {
        slot_text[i][0] = '\0';
        slot_str_rc[i] = LW_AT_PARA_FAIL;
        slot_digit_rc[i] = LW_AT_PARA_FAIL;
        slot_digit_val[i] = 0;
    }
    for (i = 0U; (i < para_num) && (i < TEST_SLOT_CAP); i++) {
        const char *s = NULL;

        slot_str_rc[i] = lw_at_get_para_str(i, &s);
        if (s != NULL) {
            (void)snprintf(slot_text[i], TEST_SLOT_TEXT, "%s", s);
        }
        slot_digit_rc[i] = lw_at_get_para_digit(i, &slot_digit_val[i]);
    }
    return LW_AT_OK;
}

static const lw_at_cmd_t test_slot_cmds[] = {
    {"+SLOT", NULL, NULL, test_slot_setup, NULL},
};
static lw_at_cmd_table_t test_slot_table = {
    test_slot_cmds,
    1U,
    NULL,
};

/**
 * @brief 打印最近一次 SLOT setup 捕获的槽表
 * @param title 小节标题
 */
static void test_slot_dump(const char *title)
{
    uint8_t i;

    (void)printf("-- %s para_num=%u calls=%u\n", title,
                 (unsigned)slot_para_num, (unsigned)slot_setup_calls);
    for (i = 0U; (i < slot_para_num) && (i < TEST_SLOT_CAP); i++) {
        (void)printf("   [%u] str_rc=%d text=\"%s\" digit_rc=%d val=%ld\n",
                     (unsigned)i, (int)slot_str_rc[i], slot_text[i],
                     (int)slot_digit_rc[i], (long)slot_digit_val[i]);
    }
}

/**
 * @brief 断言槽正文与 str 结果
 * @param index 槽下标
 * @param expect 期望正文（空串表示省略）
 * @param rc     期望 str 结果码
 */
static void test_slot_expect_str(uint8_t index, const char *expect,
                                 lw_at_para_rc_t rc)
{
    TEST_CHECK(index < slot_para_num);
    TEST_CHECK(slot_str_rc[index] == rc);
    TEST_CHECK(strcmp(slot_text[index], expect) == 0);
}

/**
 * @brief 用例：设置命令拆槽字符矩阵（黑盒，经 AT+SLOT）
 */
static void test_case_slots(void)
{
    uint32_t calls_before;

    test_banner("C12 设置拆槽与取参（字符矩阵）");
    test_setup(TEST_RX_BUF_SIZE, TEST_LINE_BUF_SIZE, 0U);
    test_slot_table.next = NULL;
    slot_setup_calls = 0U;
    TEST_CHECK(lw_at_cmd_register(&test_slot_table) == LW_AT_ERR_OK);

    test_info("C12.1 普通多槽与 digit");
    TEST_RESP("AT+SLOT=115200,8,1\r\n", TEST_RSP_OK);
    test_slot_dump("plain");
    TEST_CHECK(slot_para_num == 3U);
    test_slot_expect_str(0U, "115200", LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_rc[0] == LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_val[0] == 115200);
    TEST_CHECK(slot_digit_rc[1] == LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_val[1] == 8);

    test_info("C12.2 空参数区 AT+SLOT=");
    TEST_RESP("AT+SLOT=\r\n", TEST_RSP_OK);
    test_slot_dump("empty_area");
    TEST_CHECK(slot_para_num == 1U);
    test_slot_expect_str(0U, "", LW_AT_PARA_OMITTED);
    TEST_CHECK(slot_digit_rc[0] == LW_AT_PARA_OMITTED);

    test_info("C12.3 仅逗号 / 连续空槽 / 首尾逗号");
    TEST_RESP("AT+SLOT=,\r\n", TEST_RSP_OK);
    test_slot_dump("single_comma");
    TEST_CHECK(slot_para_num == 2U);
    test_slot_expect_str(0U, "", LW_AT_PARA_OMITTED);
    test_slot_expect_str(1U, "", LW_AT_PARA_OMITTED);

    TEST_RESP("AT+SLOT=,,\r\n", TEST_RSP_OK);
    test_slot_dump("two_commas");
    TEST_CHECK(slot_para_num == 3U);
    test_slot_expect_str(0U, "", LW_AT_PARA_OMITTED);
    test_slot_expect_str(1U, "", LW_AT_PARA_OMITTED);
    test_slot_expect_str(2U, "", LW_AT_PARA_OMITTED);

    TEST_RESP("AT+SLOT=,a\r\n", TEST_RSP_OK);
    test_slot_dump("lead_comma");
    TEST_CHECK(slot_para_num == 2U);
    test_slot_expect_str(0U, "", LW_AT_PARA_OMITTED);
    test_slot_expect_str(1U, "a", LW_AT_PARA_OK);

    TEST_RESP("AT+SLOT=a,\r\n", TEST_RSP_OK);
    test_slot_dump("trail_comma");
    TEST_CHECK(slot_para_num == 2U);
    test_slot_expect_str(0U, "a", LW_AT_PARA_OK);
    test_slot_expect_str(1U, "", LW_AT_PARA_OMITTED);

    TEST_RESP("AT+SLOT=a,,b\r\n", TEST_RSP_OK);
    test_slot_dump("mid_omit");
    TEST_CHECK(slot_para_num == 3U);
    test_slot_expect_str(0U, "a", LW_AT_PARA_OK);
    test_slot_expect_str(1U, "", LW_AT_PARA_OMITTED);
    test_slot_expect_str(2U, "b", LW_AT_PARA_OK);

    test_info("C12.4 引号：空串、去引号、引号内逗号、省略占位");
    TEST_RESP("AT+SLOT=\"\"\r\n", TEST_RSP_OK);
    test_slot_dump("empty_quotes");
    TEST_CHECK(slot_para_num == 1U);
    test_slot_expect_str(0U, "", LW_AT_PARA_OMITTED);

    TEST_RESP("AT+SLOT=\"ssid\",\"pass\",,1\r\n", TEST_RSP_OK);
    test_slot_dump("wifi_style");
    TEST_CHECK(slot_para_num == 4U);
    test_slot_expect_str(0U, "ssid", LW_AT_PARA_OK);
    test_slot_expect_str(1U, "pass", LW_AT_PARA_OK);
    test_slot_expect_str(2U, "", LW_AT_PARA_OMITTED);
    TEST_CHECK(slot_digit_rc[3] == LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_val[3] == 1);

    TEST_RESP("AT+SLOT=\"a,b\",c\r\n", TEST_RSP_OK);
    test_slot_dump("comma_in_quotes");
    TEST_CHECK(slot_para_num == 2U);
    test_slot_expect_str(0U, "a,b", LW_AT_PARA_OK);
    test_slot_expect_str(1U, "c", LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_rc[1] == LW_AT_PARA_FAIL);

    test_info("C12.5 转义：引号内与引号外");
    TEST_RESP("AT+SLOT=\"comma\\,backslash\\\\ssid\",\"x\"\r\n", TEST_RSP_OK);
    test_slot_dump("esc_in_quotes");
    TEST_CHECK(slot_para_num == 2U);
    test_slot_expect_str(0U, "comma,backslash\\ssid", LW_AT_PARA_OK);
    test_slot_expect_str(1U, "x", LW_AT_PARA_OK);

    TEST_RESP("AT+SLOT=a\\,b,c\r\n", TEST_RSP_OK);
    test_slot_dump("esc_comma_outside");
    TEST_CHECK(slot_para_num == 2U);
    test_slot_expect_str(0U, "a,b", LW_AT_PARA_OK);
    test_slot_expect_str(1U, "c", LW_AT_PARA_OK);

    TEST_RESP("AT+SLOT=\"say\\\"hi\"\r\n", TEST_RSP_OK);
    test_slot_dump("esc_quote");
    TEST_CHECK(slot_para_num == 1U);
    test_slot_expect_str(0U, "say\"hi", LW_AT_PARA_OK);

    TEST_RESP("AT+SLOT=\\z\r\n", TEST_RSP_OK);
    test_slot_dump("esc_any");
    TEST_CHECK(slot_para_num == 1U);
    test_slot_expect_str(0U, "z", LW_AT_PARA_OK);

    test_info("C12.6 嵌套引号正文（类 JSON 槽）");
    TEST_RESP("AT+SLOT=0,\"topic\",\"\\\"{\\\"sensor\\\":012}\\\"\",1,0\r\n",
              TEST_RSP_OK);
    test_slot_dump("nested_jsonish");
    TEST_CHECK(slot_para_num == 5U);
    test_slot_expect_str(0U, "0", LW_AT_PARA_OK);
    test_slot_expect_str(1U, "topic", LW_AT_PARA_OK);
    test_slot_expect_str(2U, "\"{\"sensor\":012}\"", LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_rc[3] == LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_val[3] == 1);
    TEST_CHECK(slot_digit_rc[4] == LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_val[4] == 0);

    test_info("C12.7 digit：引号整数 OK；非整型 FAIL");
    TEST_RESP("AT+SLOT=\"42\"\r\n", TEST_RSP_OK);
    test_slot_dump("quoted_int");
    TEST_CHECK(slot_para_num == 1U);
    test_slot_expect_str(0U, "42", LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_rc[0] == LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_val[0] == 42);

    TEST_RESP("AT+SLOT=12a\r\n", TEST_RSP_OK);
    test_slot_dump("bad_digit");
    TEST_CHECK(slot_para_num == 1U);
    test_slot_expect_str(0U, "12a", LW_AT_PARA_OK);
    TEST_CHECK(slot_digit_rc[0] == LW_AT_PARA_FAIL);

    TEST_RESP("AT+ECHO=\"42\"\r\n", TEST_RSP_OK);
    TEST_CHECK(test_cmd_echo_get() == 42);

    test_info("C12.8 非法：未闭合引号 / 残缺转义 / 超 ARG_MAX → ERROR 且不调 setup");
    calls_before = slot_setup_calls;
    TEST_RESP("AT+SLOT=\"abc\r\n", TEST_RSP_ERROR);
    TEST_CHECK(slot_setup_calls == calls_before);

    calls_before = slot_setup_calls;
    TEST_RESP("AT+SLOT=abc\\\r\n", TEST_RSP_ERROR);
    TEST_CHECK(slot_setup_calls == calls_before);

    calls_before = slot_setup_calls;
    TEST_RESP("AT+SLOT=1,2,3,4,5,6,7,8,9\r\n", TEST_RSP_ERROR);
    TEST_CHECK(slot_setup_calls == calls_before);

    test_info("C12.9 恰好 ARG_MAX 个槽仍成功");
    TEST_RESP("AT+SLOT=1,2,3,4,5,6,7,8\r\n", TEST_RSP_OK);
    test_slot_dump("arg_max");
    TEST_CHECK(slot_para_num == 8U);
    test_slot_expect_str(0U, "1", LW_AT_PARA_OK);
    test_slot_expect_str(7U, "8", LW_AT_PARA_OK);
}

/**
 * @brief 测试入口：依次运行全部用例并输出统计
 * @return 0 全部通过；1 存在失败用例
 */
int main(void)
{
    (void)printf("lw_at_core unit/integration test\n");
    (void)printf("see TEST.md for items / reasons / expected results\n");

    test_case_init();
    test_case_forms();
    test_case_errors();
    test_case_fragment();
    test_case_idle();
    test_case_overflow();
    test_case_line_long();
    test_case_slots();
    test_case_transmit_basic();
    test_case_transmit_guard();
    test_case_transmit_exit_harden();
    test_case_data_fixed();

    (void)printf("\n==== 汇总 ====\n");
    (void)printf("checks: %u, failed: %u\n", (unsigned)check_total,
                 (unsigned)check_fail);
    return (check_fail == 0U) ? 0 : 1;
}
