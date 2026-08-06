# LW-AT API 参考

---

| 日期 | 作者 | 说明 |
| :---: | :---: | --- |
| 2026.08.01 | linzhiwei | API 参考初版 |
| | | |

应用层仅需 `#include "lw_at.h"`。

## 类型定义

| 类型 | 说明 |
| --- | --- |
| `at_rc_t` | 命令 handler 执行结果码：`LW_AT_OK`（回 OK）/ `LW_AT_RESULT_DONE`（handler 自写结果，库不追加）/ `LW_AT_ERROR`（回 ERROR） |
| `lw_at_err_t` | 库 API 错误码：`LW_AT_ERR_OK` / `LW_AT_ERR_PARAM` / `LW_AT_ERR_STATE` |
| `lw_at_para_rc_t` | setup 取参结果码：`LW_AT_PARA_OK` / `LW_AT_PARA_OMITTED` / `LW_AT_PARA_FAIL` |
| `lw_at_cmd_t` | AT 命令描述符：`name` + `test`/`query`/`setup`/`exe` 四个 handler |
| `lw_at_cmd_table_t` | 命令表链表节点（用户静态提供，库只改写 next） |
| `lw_at_port_ops_t` | 板级适配：`write` / `timer_arm` / `timer_stop` |
| `lw_at_idle_notify_cb_t` | 空闲通知回调 |
| `lw_at_data_chunk_cb_t` / `lw_at_sink_cb_t` | 数据模式分片回调（流式/定长共用） |
| `lw_at_data_done_cb_t` | 定长窗口完成回调 |
| `lw_at_data_policy_t` | 数据模式策略：`LW_AT_DATA_STREAM`（流式）/ `LW_AT_DATA_FIXED`（定长） |
| `lw_at_data_enter_cfg_t` | 请求进入数据模式的参数 |
| `lw_at_user_cbs_t` | 应用侧回调集合：`idle_cb` / `sink` |
| `lw_at_config_t` | 库配置：rx/line/tx 缓冲区 + port + cbs + 超时 + cmd_ctx |
| `lw_at_timer_cb_t` | 单次软件定时器到期回调（定义于 lw_at_port.h） |

## 生命周期

| 函数 | 说明 |
| --- | --- |
| `lw_at_err_t lw_at_init(const lw_at_config_t *cfg)` | 初始化库（单实例；不做动态内存分配；配置被拷贝） |
| `void lw_at_deinit(void)` | 反初始化：停止定时器、复位状态、清空命令表 |

## 命令表注册

| 函数 | 说明 |
| --- | --- |
| `lw_at_err_t lw_at_cmd_register(lw_at_cmd_table_t *table)` | 注册一张静态命令表（可多次调用，链表挂接，含重名检查） |

## 字节入口与处理

| 函数 | 说明 |
| --- | --- |
| `int32_t lw_at_feed(const uint8_t *data, uint32_t len)` | 写入接收缓存并重载单次定时器（允许 ISR 调用） |
| `void lw_at_process(void)` | 处理接收缓存：命令模式拆行分发；数据模式交付回调（串行上下文） |

## setup 参数读取

| 函数 | 说明 |
| --- | --- |
| `const char *lw_at_arg_get(uint8_t index)` | 读取 setup 第 index 个槽的原始正文 |
| `lw_at_para_rc_t lw_at_get_para_str(uint8_t index, const char **out)` | 按字符串读取槽（区分空槽 OMITTED） |
| `lw_at_para_rc_t lw_at_get_para_digit(uint8_t index, int32_t *out)` | 按十进制整型读取槽 |

## 发送

| 函数 | 说明 |
| --- | --- |
| `int32_t lw_at_send_line(const char *fmt, ...)` | 发送一行中间信息，自动包裹 \r\n<正文>\r\n |
| `int32_t lw_at_write_raw(const uint8_t *data, uint32_t len)` | 向主机写原始字节（不自动加 \r\n） |

## 数据模式

| 函数 | 说明 |
| --- | --- |
| `lw_at_err_t lw_at_data_enter(const lw_at_data_enter_cfg_t *cfg)` | 请求进入数据模式（handler 内登记意图） |
| `lw_at_err_t lw_at_data_confirm(void)` | 确认进入数据模式：打印 >\r\n 后切入（同步/异步皆可） |
| `lw_at_err_t lw_at_data_cancel(void)` | 取消待确认的数据模式进入请求 |
| `lw_at_err_t lw_at_data_exit(void)` | 应用主动退出数据模式 |
| `lw_at_err_t lw_at_transmit_enter(void)` | 请求进入流式透传（等价 STREAM + cfg.cbs.sink） |
