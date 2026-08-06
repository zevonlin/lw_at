/**
 * @file lw_at_transmit.c
 * @brief LW-AT 透传实现：sink 转发与 +++ 静默守卫检测
 *
 * @details
 * 本文件恒编译（不设裁剪开关）。全部函数针对传入的
 * lw_at_transmit_t / lw_at_stream_t 实例操作，无全局状态，不感知工作
 * 模式：feed 路径的候选 '+' 暂存与还原通过 silent 标志判定前后静默；
 * 不进行时间轮询。silent 由上层（core）在 guard 定时器到期时置起，
 * 每次 feed 末尾清零并重载定时器。
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
#include "lw_at.h"

#include "lw_at_transmit.h"

/* 透传退出序列 "+++" 的长度 */
#define TRANSMIT_PLUS_SEQ_LEN 3U

/**
 * @brief 退出序列被破坏时，把暂存的 '+' 还原写回接收流
 * @param tm     透传实例
 * @param stream 接收流实例
 */
static void transmit_flush_plus(lw_at_transmit_t *tm, lw_at_stream_t *stream)
{
    /* 单字节 '+' 字面量，供还原写入 stream */
    static const uint8_t plus = (uint8_t)'+';

    while (tm->plus_cnt > 0U) {
        /* 还原时缓存若已满，按普通数据一样丢弃（overflow 标志已置） */
        (void)lw_at_stream_feed(stream, &plus, 1U);
        tm->plus_cnt--;
    }
}

/**
 * @brief 静态初始化透传状态
 */
void lw_at_transmit_init(lw_at_transmit_t *tm, uint32_t guard_ms)
{
    tm->guard_ms = guard_ms;
    tm->plus_cnt = 0U;
}

/**
 * @brief 复位透传状态
 */
void lw_at_transmit_reset(lw_at_transmit_t *tm)
{
    tm->plus_cnt = 0U;
}

/**
 * @brief 透传 feed 路径：+++ 守卫检测，普通数据写入 stream
 *
 * 前后静默由 silent 标志判定：1 表示 guard 定时器已到期（静默满足）。
 * guard_ms 为 0 时 silent 恒为 1（由上层确保），即关闭守卫。
 */
int32_t lw_at_transmit_feed(lw_at_transmit_t *tm, lw_at_stream_t *stream,
                            const uint8_t *data, uint32_t len, uint8_t silent,
                            uint8_t *exited)
{
    /* 本段接受字节数（含暂存未入缓存的 '+'） */
    int32_t written = 0;
    uint32_t i;

    *exited = 0U;

    /* 已凑齐 +++：silent 判定后静默是否满足 */
    if (tm->plus_cnt == TRANSMIT_PLUS_SEQ_LEN) {
        if (silent != 0U) {
            /* 后静默满足：仅上报退出，本段留给上层在记下分界后写入 */
            tm->plus_cnt = 0U;
            *exited = 1U;
            return 0;
        }
        /* 后静默被破坏：+++ 还原为普通数据 */
        transmit_flush_plus(tm, stream);
    }

    for (i = 0U; i < len; i++) {
        /* 当前字节：用于判定是否延续/破坏 +++ 候选序列 */
        uint8_t b = data[i];

        if (tm->plus_cnt == 0U) {
            /* 候选序列只能由「前静默后本段首字节的 '+'」开启 */
            if ((b == (uint8_t)'+') && (i == 0U) && (silent != 0U)) {
                tm->plus_cnt = 1U;
                written++;
                continue;
            }
            /* 普通数据：剩余整段一次写入 */
            written += lw_at_stream_feed(stream, &data[i], len - i);
            break;
        }
        if ((tm->plus_cnt < TRANSMIT_PLUS_SEQ_LEN) && (b == (uint8_t)'+')) {
            tm->plus_cnt++;
            written++;
            continue;
        }
        /* 序列被破坏（含凑齐后同段又来字节）：还原暂存并按普通数据处理 */
        transmit_flush_plus(tm, stream);
        written += lw_at_stream_feed(stream, &data[i], len - i);
        break;
    }
    return written;
}

/**
 * @brief 透传 process 路径：整段转发给 sink
 */
void lw_at_transmit_process(lw_at_stream_t *stream, lw_at_sink_cb_t sink,
                            void *sink_user)
{
    /* peek 给出的连续可读段起点（零拷贝） */
    const uint8_t *data;
    /* 本段连续可读字节数 */
    uint32_t n;

    /* 透传下缓存满只丢弃不回 ERROR，避免污染用户数据流 */
    (void)lw_at_stream_overflow_take(stream);
    if (sink == NULL) {
        lw_at_stream_reset(stream);
        return;
    }
    while ((n = lw_at_stream_peek(stream, &data)) > 0U) {
        sink(data, n, sink_user);
        lw_at_stream_consume(stream, n);
    }
}
