/**
 * @file lw_at.h
 * @brief LW-AT AT 指令解析库对外 API
 *
 * @details
 * LW-AT AT 从机解析库唯一对外头文件，应用层仅需包含本文件。
 * 库为单实例、非线程安全，除 lw_at_feed 允许在 ISR 中调用外，
 * 其余 API 均在串行上下文调用；缓冲区与命令表由调用方静态提供，
 * 库内不分配内存。C 库符号经 lw_at_port.h 宏映射可替换。
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
 * Date       Author    Notes                                      version
 * 2026-08-11 linzhiwei 新增运行期 API；空闲超时作废残留；精简 @details v0.9.1
 * 2026-08-06 linzhiwei 首次发布                                    v0.9.0
 */
#ifndef LW_AT_H
#define LW_AT_H

#include "lw_at_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 命令 handler 执行结果码
 *
 * 库在 handler 返回后据此决定是否写最终结果行，以及是否切换数据模式。
 */
typedef enum {
    LW_AT_OK          = 0,  /**< 成功，库向主机回 \r\nOK\r\n */
    LW_AT_RESULT_DONE = 2,  /**< handler 已自行写完结果，库不再追加 OK/ERROR */
    LW_AT_ERROR       = -1  /**< 失败，库向主机回 \r\nERROR\r\n */
} at_rc_t;

/**
 * @brief 库 API 错误码
 */
typedef enum {
    LW_AT_ERR_OK    = 0,   /**< 成功 */
    LW_AT_ERR_PARAM = -1,  /**< 参数非法 */
    LW_AT_ERR_STATE = -2   /**< 状态不允许（未初始化、模式不符等） */
} lw_at_err_t;

/**
 * @brief setup 取参结果码
 */
typedef enum {
    LW_AT_PARA_OK      = 0,  /**< 取参成功 */
    LW_AT_PARA_OMITTED = 1,  /**< 该槽为空（省略占位） */
    LW_AT_PARA_FAIL    = -1  /**< 越界、空指针或类型不符等 */
} lw_at_para_rc_t;

/**
 * @brief AT 命令描述符
 *
 * name 须指向静态字符串，库只保存指针不做拷贝。
 * 各 handler 的 ctx 实参统一取自 lw_at_config_t::cmd_ctx，可为 NULL。
 */
typedef struct {
    const char *name;                              /**< 如 "+UART"，不含 "AT"；裸 AT 用 "" */
    at_rc_t (*test)(void *ctx);                    /**< AT+CMD=? 处理，可为 NULL */
    at_rc_t (*query)(void *ctx);                   /**< AT+CMD? 处理，可为 NULL */
    at_rc_t (*setup)(uint8_t para_num, void *ctx); /**< AT+CMD=<…>；para_num 为槽个数；槽经 lw_at_get_para_str/digit 或 lw_at_arg_get 读取，可为 NULL */
    at_rc_t (*exe)(void *ctx);                     /**< AT+CMD 处理，可为 NULL */
} lw_at_cmd_t;

/**
 * @brief 命令表链表节点（由用户静态提供，库只改写 next 挂链）
 *
 * 不同业务模块可各自持有一张静态命令表与一个节点，初始化后多次调用
 * lw_at_cmd_register 挂到字典链表上。节点须长期有效，禁止栈上临时节点。
 */
typedef struct lw_at_cmd_table {
    const lw_at_cmd_t *cmds;       /**< 本节点命令数组，须长期有效 */
    uint16_t num;                  /**< 命令条目数，须 > 0 */
    struct lw_at_cmd_table *next;  /**< 库注册时写入；用户初始化为 NULL */
} lw_at_cmd_table_t;

/**
 * @brief 板级适配接口（Port）：主机链路发送与单次软件定时器
 */
typedef struct {
    /** @brief 必填；向主机发送 data[0..len)，返回实际发送字节数，<=0 视为传输故障 */
    int32_t (*write)(const uint8_t *data, int32_t len);

    /**
     * @brief 必填；启动/重载单次软件定时器（允许 ISR 内调用）
     *
     * 重复调用 = reload，覆盖前次。每次 lw_at_feed 后库会调用本函数
     * 以编程命令模式空闲门限或数据模式 guard 时长。
     * @param ms   定时时长（ms）；命令模式为 idle_timeout_ms，
     *             数据模式为 guard_ms；须 > 0
     * @param cb   到期回调（库内部函数），不可为 NULL
     * @param user 回调用户指针（库传 NULL）
     * @return 0 成功；负值失败
     */
    int32_t (*timer_arm)(uint32_t ms, lw_at_timer_cb_t cb, void *user);

    /** @brief 必填；停止定时器，已 arm 的不再触发。lw_at_deinit 时调用。 */
    void (*timer_stop)(void);
} lw_at_port_ops_t;

/**
 * @brief 空闲通知回调
 *
 * 命令模式下定时器到期时由定时器回调内调用（可能 ISR 上下文中）。
 * 回调默认仅作通知；是否在回调中调用 lw_at_process，由用户保证当前
 * 上下文安全（通常系置标志后由主循环调用）。
 * @param user 注册时传入的用户指针
 */
typedef void (*lw_at_idle_notify_cb_t)(void *user);

/**
 * @brief 数据模式分片回调（定长/流式共用）
 *
 * 由 lw_at_process（或退出排空路径）调用，把主机下行字节交给用户。
 * @param data 数据起始地址（仅在回调期间有效）
 * @param len  数据长度
 * @param user 进入数据模式时传入的用户指针
 */
typedef void (*lw_at_data_chunk_cb_t)(const uint8_t *data, uint32_t len,
                                     void *user);

/**
 * @brief 透传下行回调别名（兼容旧配置字段 cbs.sink）
 */
typedef lw_at_data_chunk_cb_t lw_at_sink_cb_t;

/**
 * @brief 定长窗口收满或提前退出时的完成回调
 * @param got  已交给用户的字节数
 * @param user 进入数据模式时传入的用户指针
 */
typedef void (*lw_at_data_done_cb_t)(uint32_t got, void *user);

/**
 * @brief 数据模式策略
 */
typedef enum {
    LW_AT_DATA_STREAM = 0, /**< 流式：持续转发；默认 +++ 守卫退出 */
    LW_AT_DATA_FIXED  = 1  /**< 定长：收满 length 后回命令模式、调 on_done 并回 OK */
} lw_at_data_policy_t;

/**
 * @brief 请求进入数据模式的参数（仅 handler 内调用 lw_at_data_enter）
 *
 * STREAM：须提供 on_chunk（或依赖 cfg.cbs.sink 的兼容路径
 * lw_at_transmit_enter）。FIXED：length 须 > 0；on_chunk/on_done 均可选。
 * guard_ms 为 0 时沿用 lw_at_config_t::guard_ms。
 */
typedef struct {
    lw_at_data_policy_t policy;     /**< 流式或定长 */
    uint32_t length;                /**< FIXED 目标长度；STREAM 忽略 */
    uint32_t guard_ms;              /**< +++ 静默；0 表示用配置默认值 */
    uint8_t allow_plus_abort;       /**< FIXED 时是否允许 +++ 取消；STREAM 忽略 */
    lw_at_data_chunk_cb_t on_chunk; /**< 分片回调；STREAM 必填（除非走兼容入口） */
    lw_at_data_done_cb_t on_done;   /**< 定长完成/取消；可为 NULL */
    void *user;                     /**< 回调用户指针 */
} lw_at_data_enter_cfg_t;

/**
 * @brief 应用侧回调集合（与板级 Port 相对）
 */
typedef struct {
    lw_at_idle_notify_cb_t idle_cb;  /**< 空闲通知回调，可为 NULL */
    void *idle_user;                 /**< idle_cb 的用户指针 */
    lw_at_sink_cb_t sink;            /**< 默认流式下行回调；为 NULL 时禁止 lw_at_transmit_enter */
    void *sink_user;                 /**< sink 的用户指针 */
} lw_at_user_cbs_t;

/**
 * @brief 库配置：全部缓冲区由调用方静态提供，库只保存指针
 *
 * idle_timeout_ms 在命令模式下用作 timer_arm 的时长参数。
 * guard_ms 在数据模式下用作 timer_arm 的时长参数，为 0 表示关闭守卫。
 */
typedef struct {
    uint8_t *rx_buf;                 /**< 接收环形缓存，必填；实际容量为 rx_buf_size - 1 */
    uint32_t rx_buf_size;            /**< 接收缓存字节数，须 >= 2 */
    uint8_t *line_buf;               /**< 单行命令组装区，必填；决定最大命令行长（含结尾 NUL） */
    uint32_t line_buf_size;          /**< 组装区字节数，须 > 0 */
    uint8_t *tx_buf;                 /**< lw_at_send_line 格式化区，必填 */
    uint32_t tx_buf_size;            /**< 格式化区字节数，须 > 0 */
    lw_at_port_ops_t port;           /**< 板级适配：write、timer_arm、timer_stop 均必填 */
    lw_at_user_cbs_t cbs;            /**< 应用回调：idle 可选；sink 仅数据模式开启时存在 */
    uint32_t idle_timeout_ms;        /**< 空闲阈值（如 50），须 > 0；用于 timer_arm */
    uint32_t guard_ms;               /**< 默认 +++ 前后静默；0 表示关闭守卫（不推荐），用于 timer_arm */
    void *cmd_ctx;                   /**< 传给全部 handler 的 ctx，可为 NULL */
} lw_at_config_t;

/**
 * @brief 初始化库（单实例；内部不做任何动态内存分配）
 * @param cfg 配置，内容被拷贝，调用后 cfg 本体可释放（缓冲区须保持有效）
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_PARAM 配置非法；
 *         LW_AT_ERR_STATE 已初始化（须先 lw_at_deinit）
 * @note 命令表请在初始化成功后通过 lw_at_cmd_register 挂链注册
 */
lw_at_err_t lw_at_init(const lw_at_config_t *cfg);

/**
 * @brief 反初始化：复位全部内部状态（含停止定时器、清空命令表链表头），回到未初始化
 */
void lw_at_deinit(void);

/**
 * @brief 注册一张静态命令表（可多次调用，按链表挂接）
 *
 * 节点由调用方静态提供；库做表内与跨表重名检查，通过后把头插到字典链。
 * @param table 命令表节点；table->cmds/num 须有效，table->next 建议为 NULL
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_PARAM 参数非法或重名；
 *         LW_AT_ERR_STATE 未初始化或该节点已在链上
 */
lw_at_err_t lw_at_cmd_register(lw_at_cmd_table_t *table);

/**
 * @brief 将 data[0..len) 写入库内接收缓存，并重载空闲/静默定时器后返回
 *
 * 允许在 ISR 中调用；内部不做 AT 解析、不调用 port write。
 * 每次调用末尾通过 port.timer_arm 重载单次定时器；命令模式 arm
 * idle_timeout_ms，数据模式 arm guard_ms。len 为 0 时立即返回 0，
 * 不重载定时器。数据模式下若确认 +++ 后静默，仅登记待退出，
 * 不在本函数内切换工作模式。
 * @param data 有效数据起始地址（调用期间须有效）
 * @param len  有效数据长度
 * @return 实际写入缓存的字节数；负值表示错误（如未初始化）。
 *         缓存不足时可能小于 len（未写入部分按「满缓冲」策略丢弃）
 */
int32_t lw_at_feed(const uint8_t *data, uint32_t len);

/**
 * @brief 处理接收缓存中的数据
 *
 * 命令模式下按 \r\n 拆行并分发（「待处理」标志未置起时立即返回）；
 * 空闲超时后若缓存仍有未闭合残留（无完整 \r\n 行），作废该残留并回
 * \r\nERROR\r\n（空闲超时亦作为帧结束裁断）。数据模式下把缓存数据交给
 * on_chunk（定长收满后回命令模式）。
 * 须在非收包 ISR 的串行上下文中调用（如主循环、或已确认安全的软件
 * 定时器任务）。
 */
void lw_at_process(void);

/**
 * @brief 读取 setup 第 index 个槽的原始正文（从 0 起）
 *
 * 仅在 setup handler 执行期间有效。正文已去除包裹引号并还原转义；
 * 空省略位为长度 0 的字符串。指针指向行组装区内切片，勿 free，
 * 仅在本 handler 返回前有效。
 * @param index 槽序号
 * @return 槽正文；序号越界或当前无有效槽表时返回 NULL
 */
const char *lw_at_arg_get(uint8_t index);

/**
 * @brief 按字符串读取 setup 第 index 个槽
 *
 * 仅在 setup handler 执行期间有效。成功时 *out 指向行组装区内正文。
 * @param index 槽序号
 * @param out   输出：字符串指针，不可为 NULL
 * @return LW_AT_PARA_OK 非空正文；LW_AT_PARA_OMITTED 空槽；
 *         LW_AT_PARA_FAIL 越界或 out 为 NULL
 */
lw_at_para_rc_t lw_at_get_para_str(uint8_t index, const char **out);

/**
 * @brief 按十进制整型读取 setup 第 index 个槽
 *
 * 仅在 setup handler 执行期间有效。整槽须可完整解析为有符号十进制整数。
 * @param index 槽序号
 * @param out   输出：整型值，不可为 NULL
 * @return LW_AT_PARA_OK 解析成功；LW_AT_PARA_OMITTED 空槽；
 *         LW_AT_PARA_FAIL 越界、out 为 NULL 或非整型文本
 */
lw_at_para_rc_t lw_at_get_para_digit(uint8_t index, int32_t *out);

/**
 * @brief 发送一行中间信息，自动包裹为 \r\n<正文>\r\n
 *
 * 供 handler 输出中间信息（如 "+ECHO:42"）；最终结果 OK/ERROR 仍由库
 * 依据 handler 返回值统一输出（除非返回 LW_AT_RESULT_DONE）。
 * @param fmt printf 风格格式串
 * @return 正文字节数（不含包裹的 \r\n）；负值表示错误（未初始化、
 *         格式化结果超出 tx_buf 容量等）
 */
int32_t lw_at_send_line(const char *fmt, ...);

/**
 * @brief 向主机写原始字节（不自动加 \r\n）
 * @param data 数据起始地址
 * @param len  字节数
 * @return 请求写出的字节数；负值表示错误
 */
int32_t lw_at_write_raw(const uint8_t *data, uint32_t len);

/**
 * @brief 请求进入数据模式，仅允许在命令 handler 执行期间调用
 *
 * 仅登记请求（存 enter_req）：handler 返回 LW_AT_OK 或 LW_AT_RESULT_DONE
 * 后库回最终结果行；此时如有待确认的进入请求，库拒绝后续命令直到用户
 * 调 lw_at_data_confirm（打印 >\r\n 并切入）或 lw_at_data_cancel（放弃）。
 * handler 返回 LW_AT_ERROR 且 enter_req=1 时，请求自动作废。
 * @param cfg 进入参数，不可为 NULL
 * @return LW_AT_ERR_OK 请求成功；LW_AT_ERR_PARAM 参数非法；
 *         LW_AT_ERR_STATE 未初始化或当前非命令模式
 */
lw_at_err_t lw_at_data_enter(const lw_at_data_enter_cfg_t *cfg);

/**
 * @brief 确认进入数据模式：打印 >\r\n 后切入，须在串行上下文调用
 *
 * 打印提示符后清空接收环、初始化透传状态并切入数据模式。
 * enter_req 消费后清零。仅命令模式且 enter_req=1 时有效。
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_STATE 未初始化、非命令模式或
 *         enter_req 为 0
 */
lw_at_err_t lw_at_data_confirm(void);

/**
 * @brief 取消待确认的数据模式进入请求，须在串行上下文调用
 *
 * 仅清零 enter_req，不回任何字节到主机。
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_STATE 未初始化或 enter_req 为 0
 */
lw_at_err_t lw_at_data_cancel(void);

/**
 * @brief 应用主动退出数据模式（串行上下文调用）
 *
 * 流式：排空分界前数据后回命令模式（不追加 OK）。定长：以当前已收字节
 * 调用 on_done（若已注册）后回 \r\nERROR\r\n。
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_STATE 未初始化或当前非数据模式
 */
lw_at_err_t lw_at_data_exit(void);

/**
 * @brief 请求进入流式透传（兼容封装，等价 STREAM + cfg.cbs.sink）
 *
 * 仅登记请求：handler 返回 LW_AT_OK 后，用户调 lw_at_data_confirm
 * 切入流式数据模式。sink 未注册时失败。
 * @return LW_AT_ERR_OK 请求成功；LW_AT_ERR_STATE 未初始化、非命令模式
 *         或 sink 未注册
 */
lw_at_err_t lw_at_transmit_enter(void);

/**
 * @brief 查询当前工作模式
 *
 * 供应用层在收到业务事件时判断透传/命令分派（如 BLE 写分派）。
 * @return LW_AT_MODE_COMMAND(0) 命令模式；LW_AT_MODE_DATA(1) 数据模式
 */
uint8_t lw_at_mode_get(void);

/**
 * @brief 查询默认 +++ 前后静默时长（guard_ms）
 * @return 静默时长（ms）；0 表示关闭守卫
 */
uint32_t lw_at_guard_get(void);

/**
 * @brief 设置默认 +++ 前后静默时长（guard_ms）
 *
 * 修改 lw_at_config_t::guard_ms 默认值，供此后进入的数据模式会话使用；
 * 单次会话仍可用 lw_at_data_enter_cfg_t::guard_ms 覆盖。
 * @param ms 静默时长（ms）；0 表示关闭守卫（不推荐）
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_STATE 未初始化
 */
lw_at_err_t lw_at_guard_set(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* LW_AT_H */
