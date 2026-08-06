/**
 * @file main.c
 * @brief LW-AT 最小可运行宿主机演示（非库核心）
 *
 * @details
 * Windows 上模拟 MCU 主循环：feed 模拟 UART RX IRQ，主循环 Sleep +
 * lw_at_process。空闲与静默由 Windows 线程池定时器（CreateTimerQueueTimer）
 * 实现事件驱动：lw_at_feed 每次喂数后自动 arm 定时器；定时器到期由
 * 线程池回调置 pending/silent 标志。主循环仅需等待空闲门限后调用
 * lw_at_process 消费。不再需要 lw_at_tick 轮询。
 *
 * 数据模式进入采用两阶段确认：handler 登记 enter_req 后返回 LW_AT_OK，
 * 库回 \r\nOK\r\n。handler 可同步调 lw_at_data_confirm 切入（> 由
 * core_exec_line 补打），也可由外部异步等待后确认（> 由 confirm 自打）。
 * 演示命令：AT / INT / STR / MIX / SLOT / CIPMODE / CIPSEND（定长与流式）。
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
#include <stdio.h>
#include <string.h>
#include <windows.h>

#include "lw_at.h"
#include "lw_at_cmd_ex.h"
#include "lw_at_port_win.h"

#define DEMO_RX_SIZE 128U
#define DEMO_LINE_SIZE 64U
#define DEMO_TX_SIZE 64U
#define DEMO_IDLE_MS 50U
#define DEMO_GUARD_MS 100U

static uint8_t rx_mem[DEMO_RX_SIZE];
static uint8_t line_mem[DEMO_LINE_SIZE];
static uint8_t tx_mem[DEMO_TX_SIZE];

/**
 * @brief 将不可见字符转义后打印（仅用于主机侧日志）
 * @param label 前缀标签
 * @param s     字符串
 */
static void demo_print_esc(const char *label, const char *s)
{
    (void)printf("%s \"", label);
    for (; *s != '\0'; s++) {
        if (*s == '\r') {
            (void)printf("\\r");
        } else if (*s == '\n') {
            (void)printf("\\n");
        } else {
            (void)putchar(*s);
        }
    }
    (void)printf("\"\n");
}

/**
 * @brief 等待空闲门限后调 lw_at_process 消费（模拟 MCU while(1)）
 *
 * lw_at_feed 已通过 port.timer_arm 自动 arm 线程池定时器；
 * 定时器到期后线程池回调会置 pending/silent 标志。
 * 本函数仅需等待 idle_ms 确保定时器已到期，再调 process 消费。
 * @param idle_ms 空闲门限
 */
static void demo_pump(uint32_t idle_ms)
{
    Sleep(idle_ms);
    lw_at_process();
}

/**
 * @brief 发送一行命令并泵主循环直到应答写出
 * @param cmd 须含 \\r\\n 的命令；定长/透传负载也可无行结束符
 */
static void demo_send(const char *cmd)
{
    demo_print_esc("host>>", cmd);
    (void)lw_at_feed((const uint8_t *)cmd, (uint32_t)strlen(cmd));
    demo_pump(DEMO_IDLE_MS);
}

/**
 * @brief 流式透传 sink：把「发往对端」的数据打到控制台（假发送）
 * @param data 数据
 * @param len  长度
 * @param user 未使用
 */
static void demo_sink(const uint8_t *data, uint32_t len, void *user)
{
    uint32_t i;

    (void)user;
    (void)printf("sink>> \"");
    for (i = 0U; i < len; i++) {
        char c = (char)data[i];

        if (c == '\r') {
            (void)printf("\\r");
        } else if (c == '\n') {
            (void)printf("\\n");
        } else {
            (void)putchar(c);
        }
    }
    (void)printf("\"\n");
}

/**
 * @brief 等待静默（供 +++ 前后守卫）
 * @param ms 毫秒
 */
static void demo_silence(uint32_t ms)
{
    Sleep(ms);
    lw_at_process();
}

int main(void)
{
    lw_at_config_t cfg;

    (void)printf("LW-AT example (Windows threadpool timer + MCU-style loop)\n");
    /* 关闭 stdout 缓冲：port_win_write 用 fflush，printf 用无缓冲，
     * 避免二者在管道环境下输出交错 */
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)printf("cmds: AT / INT / STR / MIX / SLOT");
    (void)printf(" / CIPMODE / CIPSEND");
    (void)printf("\n\n");

    (void)memset(&cfg, 0, sizeof(cfg));
    cfg.rx_buf = rx_mem;
    cfg.rx_buf_size = DEMO_RX_SIZE;
    cfg.line_buf = line_mem;
    cfg.line_buf_size = DEMO_LINE_SIZE;
    cfg.tx_buf = tx_mem;
    cfg.tx_buf_size = DEMO_TX_SIZE;
    cfg.port.write = port_win_write;
    cfg.port.timer_arm = port_win_timer_arm;
    cfg.port.timer_stop = port_win_timer_stop;
    cfg.idle_timeout_ms = DEMO_IDLE_MS;
    cfg.guard_ms = DEMO_GUARD_MS;
    cfg.cbs.sink = demo_sink;

    if (lw_at_init(&cfg) != LW_AT_ERR_OK) {
        (void)printf("lw_at_init failed\n");
        return 1;
    }
    if (lw_at_cmd_ex_register() != LW_AT_ERR_OK) {
        (void)printf("lw_at_cmd_ex_register failed\n");
        return 1;
    }

    demo_send("AT\r\n");
    demo_send("AT+INT=42\r\n");
    demo_send("AT+INT?\r\n");
    demo_send("AT+STR=hello\r\n");
    demo_send("AT+STR?\r\n");
    demo_send("AT+MIX=7,world\r\n");
    demo_send("AT+MIX?\r\n");
    /* 中间空槽：left=1, mid omitted, right=3 */
    demo_send("AT+SLOT=1,,3\r\n");
    demo_send("AT+SLOT?\r\n");
    demo_send("AT+HELLO?\r\n");
    /* 定长：handler 内同步 confirm，> 由 core_exec_line 补打 */
    demo_send("AT+CIPSEND=5\r\n");
    demo_send("hello");

    /* CIPMODE=0 时无参 CIPSEND 应失败 */
    demo_send("AT+CIPMODE?\r\n");
    demo_send("AT+CIPSEND\r\n");

    /* 异步：先配模式，handler 仅登记，外部等待后 confirm */
    demo_send("AT+CIPMODE=1\r\n");
    demo_send("AT+CIPMODE?\r\n");
    demo_send("AT+CIPSEND\r\n");
    demo_send("AT+SLOT?\r\n");
    demo_send("AT+CIPMODE=0\r\n");
    Sleep(20U);                     /* 模拟等待网络/联网就绪 */
    lw_at_data_confirm();           /* 外部异步确认：打印 >\r\n 并切入 */
    demo_send("ping");
    demo_silence(DEMO_GUARD_MS);
    demo_send("+++");
    demo_silence(DEMO_GUARD_MS);
    demo_send("AT\r\n");

    lw_at_deinit();
    (void)printf("\ndemo done.\n");
    return 0;
}
