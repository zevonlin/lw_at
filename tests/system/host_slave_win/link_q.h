/**
 * @file link_q.h
 * @brief 主机↔从机字节链路队列（Windows 线程安全）
 *
 * @details
 * 模拟 UART 双向字节流：主机写下行、读上行；从机读下行、写上行。
 * 内部环形缓冲 + CRITICAL_SECTION，供双线程异步交互测试使用。
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
#ifndef HOST_SLAVE_LINK_Q_H
#define HOST_SLAVE_LINK_Q_H

#include <stdint.h>
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 单方向链路环缓容量（字节） */
#ifndef LINK_Q_SIZE
#define LINK_Q_SIZE 4096U
#endif

/**
 * @brief 线程安全字节 FIFO
 */
typedef struct {
    uint8_t buf[LINK_Q_SIZE]; /**< 环形数据区 */
    uint32_t head;            /**< 写位置 */
    uint32_t tail;            /**< 读位置 */
    uint32_t used;            /**< 已用字节数 */
    uint32_t drop_count;      /**< 因满丢弃的累计字节（模拟拥塞） */
    CRITICAL_SECTION lock;    /**< 互斥 */
    HANDLE not_empty;         /**< 有数据事件 */
} link_q_t;

/**
 * @brief 初始化队列
 * @param q 队列
 */
void link_q_init(link_q_t *q);

/**
 * @brief 销毁队列（释放同步对象）
 * @param q 队列
 */
void link_q_deinit(link_q_t *q);

/**
 * @brief 清空队列并复位丢弃计数
 * @param q 队列
 */
void link_q_reset(link_q_t *q);

/**
 * @brief 写入字节（满则丢弃剩余并累计 drop_count）
 * @param q    队列
 * @param data 数据
 * @param len  长度
 * @return 实际写入字节数
 */
uint32_t link_q_write(link_q_t *q, const uint8_t *data, uint32_t len);

/**
 * @brief 非阻塞读取
 * @param q    队列
 * @param out  输出缓冲
 * @param max  最大读取长度
 * @return 实际读出字节数
 */
uint32_t link_q_read(link_q_t *q, uint8_t *out, uint32_t max);

/**
 * @brief 阻塞等待直到有数据或超时，再尽量读出
 * @param q          队列
 * @param out        输出缓冲
 * @param max        最大读取长度
 * @param timeout_ms 超时（ms）；INFINITE 表示一直等
 * @return 实际读出字节数；超时且无数据返回 0
 */
uint32_t link_q_read_wait(link_q_t *q, uint8_t *out, uint32_t max,
                          DWORD timeout_ms);

/**
 * @brief 当前缓冲中字节数
 * @param q 队列
 * @return 已用字节数
 */
uint32_t link_q_used(link_q_t *q);

/**
 * @brief 累计因满丢弃的字节数
 * @param q 队列
 * @return 丢弃计数
 */
uint32_t link_q_drop_count(link_q_t *q);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_LINK_Q_H */
