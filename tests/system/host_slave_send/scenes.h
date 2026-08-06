/**
 * @file scenes.h
 * @brief host_slave_send 场景入口声明
 *
 * @details
 * @note Encoding for Chinese Comments :UTF8 (no BOM)
 *
 * @author linzhiwei(zevonlin)
 * @email zevonlin@gmail.com
 * @date 2026-07-31
 * @version 1.0.0
 *
 * @copyright Copyright (c) 2026 linzhiwei(zevonlin)
 * @license SPDX-License-Identifier: Apache-2.0
 *
 * @see https://github.com/zevonlin
 *
 * Change Logs:
 * Date       Author    Notes    version
 * 2026-07-31 linzhiwei 首次发布 v1.0.0
 */
#ifndef HOST_SLAVE_SEND_SCENES_H
#define HOST_SLAVE_SEND_SCENES_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 门控场景 G
 * @return 失败场景数
 */
int scenes_run_gate(void);

/**
 * @brief 定长场景 F
 * @return 失败场景数
 */
int scenes_run_fixed(void);

/**
 * @brief 流式场景 S
 * @return 失败场景数
 */
int scenes_run_stream(void);

/**
 * @brief 异步粘连 A + 切换 X + 打断 I + 缓冲 B
 * @return 失败场景数
 */
int scenes_run_edge(void);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_SEND_SCENES_H */
