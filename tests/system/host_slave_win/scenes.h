/**
 * @file scenes.h
 * @brief 场景入口声明
 *
 * @details
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
#ifndef HOST_SLAVE_SCENES_H
#define HOST_SLAVE_SCENES_H

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 跑一类场景，返回失败场景数 */
int scenes_run_normal(void);
int scenes_run_pipeline(void);
int scenes_run_frag(void);
int scenes_run_chars(void);
int scenes_run_noise(void);
int scenes_run_buffer(void);
int scenes_run_stress(void);
int scenes_run_transmit(void);

#ifdef __cplusplus
}
#endif

#endif /* HOST_SLAVE_SCENES_H */
