/**
 * @file test_main.c
 * @brief 异步事件脚本场景集：模拟中断喂数与主循环交错
 *
 * @details
 * 面向真实主机/链路：分片、半行、噪声、连发不等待、空闲门控、
 * 溢出、命令异常、透传 +++ 时序等。步骤以 C 事件表描述，可复现。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-30
 * @version 1.1.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                      version
 * 2026-07-30 linzhiwei 增补丢字节/粘包半行/干扰/主机连发压力场景   v1.1.0
 * 2026-07-30 linzhiwei 首次发布                                    v1.0.0
 */
#include <stdio.h>
#include <string.h>

#include "harness.h"
#include "lw_at.h"
#include "test_cmd.h"
#include "test_port.h"

#define AS_RX_SIZE 128U
#define AS_LINE_SIZE 64U
#define AS_TX_SIZE 64U
#define AS_IDLE_MS 50U
#define AS_GUARD_MS 100U
#define AS_SMALL_RX 16U
#define AS_SINK_SIZE 256U

#define AS_RSP_OK "\r\nOK\r\n"
#define AS_RSP_ERR "\r\nERROR\r\n"
#define AS_RSP_OK_PROMPT "\r\nOK\r\n>\r\n"

static uint8_t rx_mem[AS_RX_SIZE];
static uint8_t line_mem[AS_LINE_SIZE];
static uint8_t tx_mem[AS_TX_SIZE];
static lw_at_config_t cfg;

static char sink_buf[AS_SINK_SIZE];
static uint32_t sink_len;

/**
 * @brief 透传 sink 捕获
 * @param data 数据
 * @param len  长度
 * @param user 未使用
 */
static void as_sink(const uint8_t *data, uint32_t len, void *user)
{
    uint32_t i;

    (void)user;
    for (i = 0U; i < len; i++) {
        if (sink_len < (AS_SINK_SIZE - 1U)) {
            sink_buf[sink_len] = (char)data[i];
            sink_len++;
        }
    }
    sink_buf[sink_len] = '\0';
}

/**
 * @brief 重建库：命令模式 + 示例表 + sink（透传测用）
 */
static void as_setup_cmd(void)
{
    lw_at_deinit();
    test_port_out_clear();
    sink_len = 0U;
    sink_buf[0] = '\0';

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.rx_buf = rx_mem;
    cfg.rx_buf_size = AS_RX_SIZE;
    cfg.line_buf = line_mem;
    cfg.line_buf_size = AS_LINE_SIZE;
    cfg.tx_buf = tx_mem;
    cfg.tx_buf_size = AS_TX_SIZE;
    cfg.port.write = test_port_write;
    cfg.port.timer_arm = test_port_timer_arm;
    cfg.port.timer_stop = test_port_timer_stop;
    cfg.idle_timeout_ms = AS_IDLE_MS;
    cfg.guard_ms = AS_GUARD_MS;
    cfg.cbs.sink = as_sink;

    (void)lw_at_init(&cfg);
    (void)test_cmd_register();
}

/**
 * @brief 小 rx 缓存，便于溢出场景
 */
static void as_setup_small_rx(void)
{
    as_setup_cmd();
    lw_at_deinit();
    cfg.rx_buf_size = AS_SMALL_RX;
    (void)lw_at_init(&cfg);
    (void)test_cmd_register();
}

/* A01：UART 中断式 1 字节分片 + 空闲后处理 */
static const h_step_t script_a01_byte_irq[] = {
    { H_INFO, 0, "A01 逐字节 IRQ 喂入 AT+ECHO=5，中途夹 TICK" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "A" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "T" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "+" },
    { H_FEED, 0, "E" },
    { H_FEED, 0, "C" },
    { H_FEED, 0, "H" },
    { H_FEED, 0, "O" },
    { H_FEED, 0, "=" },
    { H_FEED, 0, "5" },
    { H_FEED, 0, "\r" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "\n" },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A02：完整行已到，但未空闲就 process → 无输出；再到期才 OK */
static const h_step_t script_a02_idle_gate[] = {
    { H_INFO, 0, "A02 空闲门控：有完整行也须 tick 到期" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_TICK, AS_IDLE_MS - 1U, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_TICK, 1, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A03：process 前又来中断半包 — 先处理上一完整行，半包留下 */
static const h_step_t script_a03_irq_before_process[] = {
    { H_INFO, 0, "A03 完整行后、process 前 IRQ 再塞半包" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_FEED, 0, "AT+ECHO=" },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "9\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A04：主机不等 OK，连续塞两行；一次 idle+process 连续应答 */
static const h_step_t script_a04_back_to_back[] = {
    { H_INFO, 0, "A04 主机连发两行（不等待）粘包处理" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\nAT+ECHO=1\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A05：\\r 与 \\n 分两次中断到达 */
static const h_step_t script_a05_cr_lf_split[] = {
    { H_INFO, 0, "A05 CR 与 LF 分片到达" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r" },
    { H_TICK, 5, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_FEED, 0, "\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A06：噪声前缀 + 合法命令 */
static const uint8_t noise_prefix[] = { 0x00, 0xFF, (uint8_t)'X', (uint8_t)'Y' };

static const h_step_t script_a06_noise_prefix[] = {
    { H_INFO, 0, "A06 噪声字节后跟合法 AT（噪声行 ERROR，再 OK）" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED_BIN, (uint32_t)sizeof(noise_prefix), noise_prefix },
    { H_FEED, 0, "\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_ERR },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A07：错误命令后恢复 */
static const h_step_t script_a07_bad_then_good[] = {
    { H_INFO, 0, "A07 未知命令 ERROR 后仍可 AT OK" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+NOPE\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_ERR },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A08：未闭合引号失败后下一命令仍可用 */
static const h_step_t script_a08_bad_quote_recover[] = {
    { H_INFO, 0, "A08 未闭合引号 ERROR 后恢复" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+ECHO=\"12\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_ERR },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+ECHO=3\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A09：引号内逗号分片到达 */
static const h_step_t script_a09_quoted_comma_frag[] = {
    { H_INFO, 0, "A09 引号字符串跨 IRQ 分片（含逗号）" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+ECHO=" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "\"4" },
    { H_FEED, 0, "2\"\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A10：小缓存突发溢出 → ERROR，之后可恢复 */
static const uint8_t flood[32] = {
    'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X',
    'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X',
};

static const h_step_t script_a10_overflow[] = {
    { H_INFO, 0, "A10 小 rx 突发溢出整段丢弃" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED_BIN, 32U, flood },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_ERR },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A11：半行久悬后补全（模拟用户停顿） */
static const h_step_t script_a11_user_pause[] = {
    { H_INFO, 0, "A11 用户输入停顿：半行挂起再补全" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+ECH" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_FEED, 0, "O=7\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A12：多次 process 空转不应破坏状态 */
static const h_step_t script_a12_spurious_process[] = {
    { H_INFO, 0, "A12 无 pending 时反复 process 应空操作" },
    { H_OUT_CLEAR, 0, NULL },
    { H_PROCESS, 0, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/**
 * @brief A13：透传 payload 应为 "hello"
 * @return 非 0 匹配
 */
static int as_expect_sink_hello(void)
{
    return (strcmp(sink_buf, "hello") == 0) ? 1 : 0;
}

/**
 * @brief A14：伪 +++ 应进 sink 为 "d+++"
 * @return 非 0 匹配
 */
static int as_expect_sink_d_plus(void)
{
    return (strcmp(sink_buf, "d+++") == 0) ? 1 : 0;
}

/* A13：透传进入后分片数据，再合法 +++ 退出 */
static const h_step_t script_a13_transmit_async[] = {
    { H_INFO, 0, "A13 透传：CIPMODE+CIPSEND 分片下行 + 守卫退出" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+CIPMODE=1\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+CIPSEND\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK_PROMPT },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "he" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "llo" },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_FN, 0, as_expect_sink_hello },
    { H_TICK, AS_GUARD_MS, NULL },
    { H_FEED, 0, "+++" },
    { H_TICK, AS_GUARD_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_EXPECT_FN, 0, as_expect_sink_hello },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A14：透传中短间隔 +++ 应作数据；再合法退出 */
static const h_step_t script_a14_transmit_false_plus[] = {
    { H_INFO, 0, "A14 透传前静默不足：+++ 当数据" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+CIPMODE=1\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+CIPSEND\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK_PROMPT },
    /* 须清空：否则后续 AT 的 OK 会与进入透传的应答叠在捕获缓冲里 */
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "d" },
    { H_PROCESS, 0, NULL },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "+++" },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_FN, 0, as_expect_sink_d_plus },
    { H_TICK, AS_GUARD_MS, NULL },
    { H_FEED, 0, "+++" },
    { H_TICK, AS_GUARD_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_EXPECT_FN, 0, as_expect_sink_d_plus },
    { H_END, 0, NULL },
};

/**
 * @brief A16：半行补全后 ECHO 值应为 8
 * @return 非 0 匹配
 */
static int as_expect_echo_8(void)
{
    return (test_cmd_echo_get() == 8) ? 1 : 0;
}

/* A15：RX 中途丢字节（故意不喂 'H'）→ 命令名错乱 ERROR，再恢复 */
static const h_step_t script_a15_drop_byte[] = {
    { H_INFO, 0, "A15 丢字节：AT+EC + O=5（缺 H）→ ERROR，再 AT OK" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+EC" },
    { H_TICK, 2, NULL },
    /* 真实丢包：不喂 'H'，直接续上剩余字节 */
    { H_FEED, 0, "O=5\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_ERR },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A16：粘包 = 完整行 + 半行；先处理完整行，半行等后续分片 */
static const h_step_t script_a16_sticky_half[] = {
    { H_INFO, 0, "A16 粘包半行：AT\\r\\nAT+ECHO= 一次到达" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\nAT+ECHO=" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "8\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_EXPECT_FN, 0, as_expect_echo_8 },
    { H_END, 0, NULL },
};

/*
 * A17：主机不等待从机——16 条 AT 一次塞入（压力粘包）。
 * 对应现场：主机流水线发令，从机一次 idle+process 连回多段 OK。
 */
static const char flood_at_16[] =
    "AT\r\nAT\r\nAT\r\nAT\r\n"
    "AT\r\nAT\r\nAT\r\nAT\r\n"
    "AT\r\nAT\r\nAT\r\nAT\r\n"
    "AT\r\nAT\r\nAT\r\nAT\r\n";

static const h_step_t script_a17_host_flood[] = {
    { H_INFO, 0, "A17 主机连发 16 条 AT（不等 OK）压力粘包" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, flood_at_16 },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT_COUNT, 16U, AS_RSP_OK },
    { H_END, 0, NULL },
};

/* A18：两帧合法 AT 之间插入干扰行 */
static const uint8_t mid_noise[] = { 0x01, 0x02, 0x7F };

static const h_step_t script_a18_noise_between[] = {
    { H_INFO, 0, "A18 帧间干扰：OK → 噪声行 ERROR → OK" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_FEED_BIN, (uint32_t)sizeof(mid_noise), mid_noise },
    { H_FEED, 0, "\r\nAT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT_COUNT, 2U, AS_RSP_OK },
    { H_EXPECT_OUT_COUNT, 1U, AS_RSP_ERR },
    { H_END, 0, NULL },
};

/*
 * A19：丢 LF（只有 CR）——行不闭合；随后新命令拼进同一逻辑行 → ERROR，再恢复。
 * 模拟：UART 丢了 \\n，下一帧的 AT\\r\\n 与残留拼成 AT\\rAT。
 */
static const h_step_t script_a19_lost_lf[] = {
    { H_INFO, 0, "A19 丢 LF：残留 CR 与下一帧粘成脏行" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_ERR },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/*
 * A20：主机流水线交错——先积压两行再 process；随后立刻再塞一行（不等主机读完 OK）。
 * 这是「未等待」在事件表上的正确建模：响应尚未被主机消费，下行又到。
 */
static const h_step_t script_a20_pipeline[] = {
    { H_INFO, 0, "A20 流水线：积压两行 process，再立即第三行" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\nAT\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT_COUNT, 2U, AS_RSP_OK },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT+ECHO=4\r\n" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, AS_RSP_OK },
    { H_END, 0, NULL },
};

/*
 * A21：交错压力——多段小包 FEED 夹短 TICK，最后一次 idle+process 消化。
 * 模拟高负载 UART：字节持续到达，主循环偶发才跑 process。
 */
static const h_step_t script_a21_irq_stress[] = {
    { H_INFO, 0, "A21 IRQ 压力：多片粘连 + 延迟 process" },
    { H_OUT_CLEAR, 0, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "AT+ECH" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "O=6\r\n" },
    { H_TICK, 1, NULL },
    { H_FEED, 0, "AT\r\n" },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT, 0, "" },
    { H_TICK, AS_IDLE_MS, NULL },
    { H_PROCESS, 0, NULL },
    { H_EXPECT_OUT_COUNT, 3U, AS_RSP_OK },
    { H_END, 0, NULL },
};

int main(void)
{
    int failed_scenes = 0;

    (void)printf("async_script harness\n");
    (void)printf("see TEST.md — event table simulates IRQ feed vs timer callback\n");

    failed_scenes += harness_run("A01 byte IRQ fragment", as_setup_cmd,
                                 script_a01_byte_irq);
    failed_scenes += harness_run("A02 idle gate", as_setup_cmd, script_a02_idle_gate);
    failed_scenes += harness_run("A03 IRQ before process", as_setup_cmd,
                                 script_a03_irq_before_process);
    failed_scenes += harness_run("A04 back-to-back cmds", as_setup_cmd,
                                 script_a04_back_to_back);
    failed_scenes += harness_run("A05 CR/LF split", as_setup_cmd,
                                 script_a05_cr_lf_split);
    failed_scenes += harness_run("A06 noise prefix", as_setup_cmd,
                                 script_a06_noise_prefix);
    failed_scenes += harness_run("A07 bad then good", as_setup_cmd,
                                 script_a07_bad_then_good);
    failed_scenes += harness_run("A08 quote recover", as_setup_cmd,
                                 script_a08_bad_quote_recover);
    failed_scenes += harness_run("A09 quoted frag", as_setup_cmd,
                                 script_a09_quoted_comma_frag);
    failed_scenes += harness_run("A10 overflow", as_setup_small_rx,
                                 script_a10_overflow);
    failed_scenes += harness_run("A11 user pause", as_setup_cmd,
                                 script_a11_user_pause);
    failed_scenes += harness_run("A12 spurious process", as_setup_cmd,
                                 script_a12_spurious_process);
    failed_scenes += harness_run("A13 transmit async", as_setup_cmd,
                                 script_a13_transmit_async);
    failed_scenes += harness_run("A14 false +++", as_setup_cmd,
                                 script_a14_transmit_false_plus);
    failed_scenes += harness_run("A15 drop byte", as_setup_cmd, script_a15_drop_byte);
    failed_scenes += harness_run("A16 sticky half", as_setup_cmd,
                                 script_a16_sticky_half);
    failed_scenes += harness_run("A17 host flood", as_setup_cmd,
                                 script_a17_host_flood);
    failed_scenes += harness_run("A18 noise between", as_setup_cmd,
                                 script_a18_noise_between);
    failed_scenes += harness_run("A19 lost LF", as_setup_cmd, script_a19_lost_lf);
    failed_scenes += harness_run("A20 pipeline", as_setup_cmd, script_a20_pipeline);
    failed_scenes += harness_run("A21 irq stress", as_setup_cmd,
                                 script_a21_irq_stress);

    (void)printf("\n==== 汇总 ====\n");
    (void)printf("checks: %u, failed: %u, scenes_failed: %d\n",
                 (unsigned)harness_check_count(),
                 (unsigned)harness_fail_count(), failed_scenes);
    return (harness_fail_count() == 0U) ? 0 : 1;
}
