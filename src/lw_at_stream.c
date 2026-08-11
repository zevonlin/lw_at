/**
 * @file lw_at_stream.c
 * @brief LW-AT 接收流实现：环形缓存、满丢弃、按 \r\n 取行
 *
 * @details
 * 接收流实现：环形缓存、满丢弃、按 \r\n 取行。取行仅以 \r\n 为帧
 * 边界；空闲判定由上层 lw_at_core 的定时器回调负责。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-08-11
 * @version 0.9.1
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes                                    version
 * 2026-08-11 linzhiwei 精简头注释 @details 至职责/约束/依赖          v0.9.1
 * 2026-08-06 linzhiwei 首次发布                                    v0.9.0
 */
#include <stdint.h>

#include "lw_at_stream.h"

/**
 * @brief 计算环形缓存中 idx 的下一位置
 * @param stream 接收流实例
 * @param idx    当前位置
 * @return 下一位置（回绕后）
 */
static uint32_t stream_next(const lw_at_stream_t *stream, uint32_t idx)
{
    idx++;
    if (idx >= stream->size) {
        idx = 0U;
    }
    return idx;
}

/**
 * @brief 静态初始化接收流
 */
void lw_at_stream_init(lw_at_stream_t *stream, uint8_t *buf, uint32_t size)
{
    stream->buf = buf;
    stream->size = size;
    stream->head = 0U;
    stream->tail = 0U;
    stream->overflow = 0U;
    stream->pending = 0U;
}

/**
 * @brief 丢弃缓存中全部未消费数据并清除通知类标志
 *
 * 只移动消费者侧的 tail，不触碰生产者侧状态，故与 ISR 中的喂数并发
 * 是安全的。同步清零 overflow 与 pending，避免模式切换后误用。
 */
void lw_at_stream_reset(lw_at_stream_t *stream)
{
    stream->tail = stream->head;
    stream->overflow = 0U;
    stream->pending = 0U;
}

/**
 * @brief 写入数据，缓存满时置 overflow 标志并丢弃剩余
 */
int32_t lw_at_stream_feed(lw_at_stream_t *stream, const uint8_t *data, uint32_t len)
{
    /* 本地写游标：先落盘再一次性发布到 stream->head，避免消费者读到半写状态 */
    uint32_t head = stream->head;
    /* 本段实际写入字节数，缓存满时可能小于 len */
    uint32_t written = 0U;
    uint32_t i;

    for (i = 0U; i < len; i++) {
        /* 下一写位置；若等于 tail 表示环形缓存已满 */
        uint32_t next = stream_next(stream, head);

        /* 写满（head 追上 tail 前一格）：置标志并丢弃剩余新数据 */
        if (next == stream->tail) {
            stream->overflow = 1U;
            break;
        }
        stream->buf[head] = data[i];
        head = next;
        written++;
    }

    /* 数据全部落盘后再发布 head，保证消费者不会读到未写入的字节 */
    stream->head = head;
    return (int32_t)written;
}

/**
 * @brief 取出一行正文并消费之
 */
int32_t lw_at_stream_get_line(lw_at_stream_t *stream, char *out, uint32_t out_size)
{
    /* 快照生产者写位置，扫描过程中新 feed 的数据留待下次取行 */
    uint32_t head = stream->head;
    /* 本行扫描起点，亦即当前消费者读位置 */
    uint32_t tail = stream->tail;
    /* 扫描游标，从 tail 向 head 推进 */
    uint32_t i = tail;
    /* 已扫描字节数（含尚未确认的 \r），用于计算行正文长度 */
    uint32_t scanned = 0U;
    /* 上一字节是否为 \r，用于识别 \r\n 帧边界 */
    uint8_t prev_cr = 0U;

    while (i != head) {
        uint8_t b = stream->buf[i];

        i = stream_next(stream, i);
        if ((prev_cr == 1U) && (b == (uint8_t)'\n')) {
            /* 行正文 = [tail, \r)，scanned 此刻含 \r 不含 \n */
            uint32_t line_len = scanned - 1U;

            if ((line_len + 1U) > out_size) {
                /* 行超长：连同 \r\n 一起消费丢弃，由调用方回 ERROR */
                stream->tail = i;
                return LW_AT_STREAM_LINE_LONG;
            }

            /* 从环形缓存拷贝正文到线性输出区 */
            uint32_t src = tail;
            uint32_t k;
            for (k = 0U; k < line_len; k++) {
                out[k] = (char)stream->buf[src];
                src = stream_next(stream, src);
            }
            out[line_len] = '\0';
            stream->tail = i;
            return (int32_t)line_len;
        }
        prev_cr = (b == (uint8_t)'\r') ? 1U : 0U;
        scanned++;
    }
    return LW_AT_STREAM_NO_LINE;
}

/**
 * @brief 取一段连续可读数据（零拷贝，不消费）
 */
uint32_t lw_at_stream_peek(const lw_at_stream_t *stream, const uint8_t **data)
{
    /* 读写位置快照，用于零拷贝给出一段连续可读区 */
    uint32_t head = stream->head;
    uint32_t tail = stream->tail;

    if (head == tail) {
        return 0U;
    }
    *data = &stream->buf[tail];

    /* 回绕时先给到物理末尾的一段，剩余部分由下次 peek 取得 */
    if (head > tail) {
        return head - tail;
    }
    return stream->size - tail;
}

/**
 * @brief 消费 n 字节
 */
void lw_at_stream_consume(lw_at_stream_t *stream, uint32_t n)
{
    /* 新的读位置；越过物理末尾时回绕 */
    uint32_t tail = stream->tail + n;

    if (tail >= stream->size) {
        tail -= stream->size;
    }
    stream->tail = tail;
}

/**
 * @brief 取走「待处理」标志
 */
uint8_t lw_at_stream_pending_take(lw_at_stream_t *stream)
{
    /* 取走前的待处理标志，随后清零避免重复 process */
    uint8_t v = stream->pending;

    stream->pending = 0U;
    return v;
}

/**
 * @brief 取走满丢弃标志
 */
uint8_t lw_at_stream_overflow_take(lw_at_stream_t *stream)
{
    /* 取走前的溢出标志，随后清零避免重复回 ERROR */
    uint8_t v = stream->overflow;

    stream->overflow = 0U;
    return v;
}
