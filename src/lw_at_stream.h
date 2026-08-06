/**
 * @file lw_at_stream.h
 * @brief LW-AT 接收流模块接口：环形缓存、满丢弃、取行
 *
 * @details
 * 接收流的独立状态结构体 lw_at_stream_t 与操作函数。全部函数以实例
 * 指针为第一参数，不依赖任何全局状态，可独立测试与复用。
 * 并发约定：head 仅由生产者（feed 路径，允许 ISR）修改，tail 仅由
 * 消费者（process 路径）修改，即单生产者单消费者模型。
 * 空闲定时器已改为 lw_at_timer_cb_t 事件驱动（见 lw_at_port.h），
 * 本模块不再持有时间相关成员；pending 由定时器回调置起。
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
#ifndef LW_AT_STREAM_H
#define LW_AT_STREAM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 环形缓存最小字节数（留 1 字节区分空/满） */
#define LW_AT_STREAM_SIZE_MIN 2U

/* lw_at_stream_get_line 返回：缓存中暂无完整行 */
#define LW_AT_STREAM_NO_LINE (-1)

/* lw_at_stream_get_line 返回：行长超出输出区，该行已被丢弃 */
#define LW_AT_STREAM_LINE_LONG (-2)

/**
 * @brief 接收流状态：环形缓存 + 待处理/满丢弃标志
 */
typedef struct {
    uint8_t *buf;                     /**< 环形缓存存储区（调用方静态提供） */
    uint32_t size;                    /**< 存储区字节数，实际容量为 size - 1 */
    volatile uint32_t head;           /**< 写位置，仅生产者（feed 路径）修改 */
    volatile uint32_t tail;           /**< 读位置，仅消费者（process 路径）修改 */
    volatile uint8_t overflow;        /**< 缓存满丢弃标志 */
    volatile uint8_t pending;         /**< 「待处理」标志，由定时器回调置起 */
} lw_at_stream_t;

/**
 * @brief 静态初始化接收流（不做任何动态内存分配）
 * @param stream 接收流实例
 * @param buf    环形缓存存储区
 * @param size   存储区字节数，须 >= LW_AT_STREAM_SIZE_MIN
 */
void lw_at_stream_init(lw_at_stream_t *stream, uint8_t *buf, uint32_t size);

/**
 * @brief 丢弃缓存中全部未消费数据并清除通知类标志
 *
 * 仅移动消费者侧 tail（与 ISR 喂数并发安全），同时清零 overflow 与
 * pending，避免模式切换后误用旧标志。
 * @param stream 接收流实例
 */
void lw_at_stream_reset(lw_at_stream_t *stream);

/**
 * @brief 写入数据，缓存满时置 overflow 标志并丢弃剩余
 * @param stream 接收流实例
 * @param data   数据起始地址
 * @param len    数据长度
 * @return 实际写入字节数
 */
int32_t lw_at_stream_feed(lw_at_stream_t *stream, const uint8_t *data, uint32_t len);

/**
 * @brief 取出一行正文（不含 \r\n，NUL 结尾）并消费之
 * @param stream   接收流实例
 * @param out      行输出区
 * @param out_size 输出区字节数（含 NUL）
 * @return >=0 行正文长度；LW_AT_STREAM_NO_LINE 无完整行；
 *         LW_AT_STREAM_LINE_LONG 行超长（该行已连同 \r\n 一起丢弃）
 */
int32_t lw_at_stream_get_line(lw_at_stream_t *stream, char *out, uint32_t out_size);

/**
 * @brief 取一段连续可读数据（零拷贝，不消费）
 * @param stream 接收流实例
 * @param data   输出：连续数据起始地址
 * @return 连续可读字节数，0 表示缓存空；因环形回绕，一次最多取到
 *         缓存物理末尾，调用方应循环取用
 */
uint32_t lw_at_stream_peek(const lw_at_stream_t *stream, const uint8_t **data);

/**
 * @brief 消费 n 字节（与 lw_at_stream_peek 配对使用）
 * @param stream 接收流实例
 * @param n      消费字节数，不得超过最近一次 peek 返回值
 */
void lw_at_stream_consume(lw_at_stream_t *stream, uint32_t n);

/**
 * @brief 取走「待处理」标志（读取并清零）
 * @param stream 接收流实例
 * @return 取走前的标志值
 */
uint8_t lw_at_stream_pending_take(lw_at_stream_t *stream);

/**
 * @brief 取走满丢弃标志（读取并清零）
 * @param stream 接收流实例
 * @return 取走前的标志值
 */
uint8_t lw_at_stream_overflow_take(lw_at_stream_t *stream);

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_STREAM_H */
