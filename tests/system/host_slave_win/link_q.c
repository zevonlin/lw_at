/**
 * @file link_q.c
 * @brief 主机↔从机字节链路队列实现
 *
 * @details
 * Windows CRITICAL_SECTION + 手动复位事件，实现阻塞读与非阻塞写。
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-30
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes    version
 * 2026-07-30 linzhiwei 首次发布 v1.0.0
 */
#include "link_q.h"

#include <string.h>

void link_q_init(link_q_t *q)
{
    memset(q, 0, sizeof(*q));
    InitializeCriticalSection(&q->lock);
    q->not_empty = CreateEvent(NULL, TRUE, FALSE, NULL);
}

void link_q_deinit(link_q_t *q)
{
    if (q->not_empty != NULL) {
        CloseHandle(q->not_empty);
        q->not_empty = NULL;
    }
    DeleteCriticalSection(&q->lock);
}

void link_q_reset(link_q_t *q)
{
    EnterCriticalSection(&q->lock);
    q->head = 0U;
    q->tail = 0U;
    q->used = 0U;
    q->drop_count = 0U;
    ResetEvent(q->not_empty);
    LeaveCriticalSection(&q->lock);
}

uint32_t link_q_write(link_q_t *q, const uint8_t *data, uint32_t len)
{
    uint32_t i;
    uint32_t written = 0U;

    if ((data == NULL) || (len == 0U)) {
        return 0U;
    }

    EnterCriticalSection(&q->lock);
    for (i = 0U; i < len; i++) {
        if (q->used >= LINK_Q_SIZE) {
            q->drop_count += (len - i);
            break;
        }
        q->buf[q->head] = data[i];
        q->head++;
        if (q->head >= LINK_Q_SIZE) {
            q->head = 0U;
        }
        q->used++;
        written++;
    }
    if (q->used > 0U) {
        SetEvent(q->not_empty);
    }
    LeaveCriticalSection(&q->lock);
    return written;
}

uint32_t link_q_read(link_q_t *q, uint8_t *out, uint32_t max)
{
    uint32_t n = 0U;

    if ((out == NULL) || (max == 0U)) {
        return 0U;
    }

    EnterCriticalSection(&q->lock);
    while ((n < max) && (q->used > 0U)) {
        out[n] = q->buf[q->tail];
        q->tail++;
        if (q->tail >= LINK_Q_SIZE) {
            q->tail = 0U;
        }
        q->used--;
        n++;
    }
    if (q->used == 0U) {
        ResetEvent(q->not_empty);
    }
    LeaveCriticalSection(&q->lock);
    return n;
}

uint32_t link_q_read_wait(link_q_t *q, uint8_t *out, uint32_t max,
                          DWORD timeout_ms)
{
    DWORD wr;

    if (link_q_used(q) == 0U) {
        wr = WaitForSingleObject(q->not_empty, timeout_ms);
        if (wr != WAIT_OBJECT_0) {
            return 0U;
        }
    }
    return link_q_read(q, out, max);
}

uint32_t link_q_used(link_q_t *q)
{
    uint32_t n;

    EnterCriticalSection(&q->lock);
    n = q->used;
    LeaveCriticalSection(&q->lock);
    return n;
}

uint32_t link_q_drop_count(link_q_t *q)
{
    uint32_t n;

    EnterCriticalSection(&q->lock);
    n = q->drop_count;
    LeaveCriticalSection(&q->lock);
    return n;
}
