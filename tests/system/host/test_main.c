/**
 * @file test_main.c
 * @brief LW-AT 宿主机黑盒测试（tests/system/host）
 *
 * @details
 * 基于 tests/fixtures（输出捕获 + 虚拟定时器 + 测试命令）对库做黑盒
 * 功能验证，并打印主机↔从机交互流程。覆盖：初始化与命令表链表注册、
 * 四种命令形态、错误行、分片喂数、空闲通知、缓存满、行超长、透传。
 * Makefile 编译后运行，进程返回非 0 表示存在失败用例。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 1.7.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-08-11 linzhiwei 流式退出断言改为期望自动回 OK                 v1.7.0
 * 2026-08-01 linzhiwei 移除 LW_AT_CFG_TRANSMIT 条件编译             v1.6.0
 * 2026-08-01 linzhiwei 事件驱动适配：虚拟定时器；退出由 process 完成 v1.5.0
 * 2026-07-30 linzhiwei Port/命令改挂 tests/fixtures               v1.4.0
 * 2026-07-30 linzhiwei 迁入 tests/system/host，依赖 examples/port|cmd   v1.3.0
 * 2026-07-30 linzhiwei 透传用例按 LW_AT_CFG_TRANSMIT 裁剪         v1.2.0
 * 2026-07-30 linzhiwei 为有意义局部变量补充注释                   v1.1.1
 * 2026-07-30 linzhiwei 适配链表注册；打印交互流程                 v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
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
#define TEST_GUARD_MS 100U

/* 不足守卫/空闲阈值的短间隔（ms） */
#define TEST_SHORT_MS 1U

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

    test_banner("初始化与命令表链表注册");

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
    test_banner("四种命令形态");
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
    test_banner("错误行与空行");
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
    test_banner("分片喂数与多行处理");
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
    test_banner("空闲定时与待处理标志");
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
}

/**
 * @brief 用例：缓存满丢弃新数据，处理时回 ERROR 并清空整段缓存
 */
static void test_case_overflow(void)
{
    /* 超量垃圾数据，用于触发接收环形缓存满 */
    char junk[TEST_OVERFLOW_FEED_LEN];

    test_banner("接收缓存满");
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
    test_banner("行超长");
    test_setup(TEST_RX_BUF_SIZE, TEST_SMALL_LINE_SIZE, 0U);
    TEST_RESP("AT+ECHO=1234\r\n", TEST_RSP_ERROR);
    TEST_RESP("AT\r\n", TEST_RSP_OK);
}

/**
 * @brief 用例：透传进入、半行作废、下行转发与正常退出
 */
static void test_case_transmit_basic(void)
{
    test_banner("透传进入/转发/退出");
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
    test_info("静默 + +++ + 静默，经定时器回调 + process 退出，默认自动回 OK");
    test_port_tick_advance(TEST_GUARD_MS);
    (void)test_feed("+++");
    test_port_tick_advance(TEST_GUARD_MS);
    lw_at_process();
    (void)printf("<< \"%s\"\n", test_esc(test_port_out_get()));
    TEST_CHECK(strcmp(test_port_out_get(), TEST_RSP_OK) == 0);
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
    test_banner("透传 +++ 守卫");
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
}

/**
 * @brief 测试入口：依次运行全部用例并输出统计
 * @return 0 全部通过；1 存在失败用例
 */
int main(void)
{
    (void)printf("LW-AT host test\n");

    test_case_init();
    test_case_forms();
    test_case_errors();
    test_case_fragment();
    test_case_idle();
    test_case_overflow();
    test_case_line_long();
    test_case_transmit_basic();
    test_case_transmit_guard();

    (void)printf("\n==== 汇总 ====\n");
    (void)printf("checks: %u, failed: %u\n", (unsigned)check_total,
                 (unsigned)check_fail);
    return (check_fail == 0U) ? 0 : 1;
}
