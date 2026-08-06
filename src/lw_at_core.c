/**
 * @file lw_at_core.c
 * @brief LW-AT 核心：生命周期、模式管理、行解析分派与结果回包
 *
 * @details
 * 持有库静态单例 lw_at_t（对外 API 签名由需求文档钦定为单实例，
 * 单例仅存在于本文件；各子模块函数均以实例指针传参，可多实例复用）。
 * feed 只入缓存并重载单次定时器（数据模式可先经 +++ 守卫；确认退出时
 * 仅置 exit_req，不在 ISR 内切模式）；process 在串行上下文完成排空与
 * 模式切换。空闲与静默改为事件驱动：at_timer_expired 由 port.timer_arm
 * 注册为到期回调，仅置标志不做解析/write。
 *
 * 数据模式进入采用两阶段确认：handler 调 lw_at_data_enter 登记后返回
 * LW_AT_OK；库回 \r\nOK\r\n 但不切换模式；用户准备完毕后调用
 * lw_at_data_confirm 打印 >\r\n 并切入数据模式。未确认前库拒绝后续命令。
 *
 * process 在命令模式下按行取出、拆解 AT 命令并按 handler 结果码回包
 * （含 OK、RESULT_DONE）；数据模式支持流式与定长窗口。设置参数区按
 * 引号/转义约定原地拆槽。缓存满按约定回 ERROR 并丢弃整段缓存。
 * 内核不内置业务命令名。
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
#include "lw_at_cmd_dict.h"
#include "lw_at_core.h"

/* 命令行固定前缀 "AT" 的长度 */
#define LW_AT_PREFIX_LEN 2U

/* 十进制解析基数，供 lw_at_get_para_digit 使用 */
#define LW_AT_PARA_DEC_BASE 10

/* 透传退出序列 "+++" 的长度（供 at_timer_expired 使用） */
#define LW_AT_PLUS_SEQ_LEN 3U

/* 库静态单例：仅本文件可见；对外 API 无实例指针，内部一律经此访问 */
static lw_at_t at_instance;

/* handler 执行上下文标记：置 1 时表示 lw_at_data_confirm 由 handler 同步调用 */
static volatile uint8_t core_in_handler;

/* 最终成功结果行的确切字节形式：\r\nOK\r\n */
static const char core_rsp_ok[] = "\r\nOK\r\n";

/* 最终失败结果行的确切字节形式：\r\nERROR\r\n */
static const char core_rsp_error[] = "\r\nERROR\r\n";

/* 应答行包裹用的行结束符：\r\n */
static const char core_crlf[] = "\r\n";

/* 数据模式输入提示：OK 之后单独一行的输入提示（含行结束） */
static const char core_input_prompt[] = ">\r\n";

/**
 * @brief 经 Port write 尽量写完整段数据，写失败（<=0）时放弃剩余
 * @param at   库实例
 * @param data 数据起始地址
 * @param len  数据长度
 */
static void core_write_all(lw_at_t *at, const uint8_t *data, uint32_t len)
{
    while (len > 0U) {
        /* 本轮 Port 实际写出字节数；<=0 视为传输故障 */
        int32_t n = at->cfg.port.write(data, (int32_t)len);

        /* 传输故障：无法补救，按约定放弃本次输出 */
        if (n <= 0) {
            return;
        }
        data = &data[n];
        len -= (uint32_t)n;
    }
}

/**
 * @brief 按 handler 结果码输出最终结果行
 * @param at 库实例
 * @param rc handler 执行结果
 */
static void core_write_final(lw_at_t *at, at_rc_t rc)
{
    if (rc == LW_AT_RESULT_DONE) {
        return;
    }
    if (rc == LW_AT_OK) {
        core_write_all(at, (const uint8_t *)core_rsp_ok, sizeof(core_rsp_ok) - 1U);
        return;
    }
    core_write_all(at, (const uint8_t *)core_rsp_error, sizeof(core_rsp_error) - 1U);
}

/**
 * @brief 登记一个已写完的槽；超限则失败
 * @param at         库实例
 * @param slot_start 槽正文起点（指向 line_buf 内）
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_PARAM 超过 LW_AT_ARG_MAX
 */
static lw_at_err_t core_slot_push(lw_at_t *at, char *slot_start)
{
    if (at->arg_num >= LW_AT_ARG_MAX) {
        at->arg_num = 0U;
        return LW_AT_ERR_PARAM;
    }
    at->args[at->arg_num] = slot_start;
    at->arg_num++;
    return LW_AT_ERR_OK;
}

/**
 * @brief 按设置参数写法约定原地拆槽并登记
 * @param at 库实例
 * @param s  参数区起始（'=' 之后），会被原地压缩改写
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_PARAM 超限、未闭合引号或残缺转义
 * @details
 * 双游标压紧：rd 读原始字节，wr 写「槽正文」。两个游标都从 s 起步且
 * wr 永不越过 rd，故可安全改写同一缓冲。结果形态为多个以 NUL 结尾的
 * 子串首尾相接，args[] 各指向其中一个子串起点。例：
 * `115200,8,"a,b"` → `115200\08\0a,b\0`，三槽指针分别指向 115200 / 8 / a,b。
 */
static lw_at_err_t core_split_slots(lw_at_t *at, char *s)
{
    /* 读游标：扫描原始参数区 */
    char *rd;
    /* 写游标：压缩写出已去引号/已还原转义的正文 */
    char *wr;
    /* 当前槽正文起点（登记进 args[] 的就是这个地址） */
    char *slot_start;
    /* 是否处于双引号字符串内部：内部逗号不当分隔符 */
    uint8_t in_quote;

    at->arg_num = 0U;
    rd = s;
    wr = s;
    slot_start = wr;
    in_quote = 0U;

    for (;;) {
        char c = *rd;

        /* 参数区结束：收尾当前槽；引号未闭合则整段非法 */
        if (c == '\0') {
            if (in_quote != 0U) {
                at->arg_num = 0U;
                return LW_AT_ERR_PARAM;
            }
            *wr = '\0';
            return core_slot_push(at, slot_start);
        }

        /*
         * 引号外的逗号才是槽分界：在写位置落 NUL 结束本槽，
         * 登记后再把下一槽起点接到 NUL 之后（wr 已前进一格）。
         * 连续逗号（如 a,,b）会登记出中间的空串槽。
         */
        if ((in_quote == 0U) && (c == ',')) {
            *wr = '\0';
            wr++;
            if (core_slot_push(at, slot_start) != LW_AT_ERR_OK) {
                return LW_AT_ERR_PARAM;
            }
            rd++;
            slot_start = wr;
            continue;
        }

        /* 引号外遇到 "：进入字符串，引号本身不写入正文 */
        if ((in_quote == 0U) && (c == '"')) {
            in_quote = 1U;
            rd++;
            continue;
        }

        /* 引号内遇到 "：离开字符串，引号本身不写入正文 */
        if ((in_quote != 0U) && (c == '"')) {
            in_quote = 0U;
            rd++;
            continue;
        }

        /*
         * 转义：跳过 '\'，把紧随其后的字符原样写入（引号内外相同）。
         * 故 \, \" \\ 分别得到字面 , " \；行末单独的 \ 视为残缺转义。
         */
        if (c == '\\') {
            rd++;
            if (*rd == '\0') {
                at->arg_num = 0U;
                return LW_AT_ERR_PARAM;
            }
            *wr = *rd;
            wr++;
            rd++;
            continue;
        }

        /* 普通字符：原样落入当前槽正文 */
        *wr = c;
        wr++;
        rd++;
    }
}

/**
 * @brief 解析一行命令正文并执行对应 handler
 * @param at   库实例
 * @param line NUL 结尾的行正文（不含 \r\n），会被原地改写
 * @return handler 执行结果；格式非法、查表失败或 handler 缺失返回 LW_AT_ERROR
 */
static at_rc_t core_parse_line(lw_at_t *at, char *line)
{
    /* 字典命中的命令描述符，未找到则为 NULL */
    const lw_at_cmd_t *cmd;
    /* "AT" 前缀之后的剩余正文（命令名 + 可选后缀） */
    char *rest;
    /* 扫描命令名与后缀分界（'=' / '?' / 行尾）的游标 */
    char *p;
    /* 分界符字符：'\0' 执行、'?' 查询、'=' 设置/测试 */
    char delim;

    /* "AT" 前缀强校验，须大写 */
    if (lw_at_strncmp(line, "AT", LW_AT_PREFIX_LEN) != 0) {
        return LW_AT_ERROR;
    }
    rest = &line[LW_AT_PREFIX_LEN];

    /* 裸 AT：匹配 name 为空串的表项 */
    if (*rest == '\0') {
        cmd = lw_at_cmd_dict_find("");
        if ((cmd == NULL) || (cmd->exe == NULL)) {
            return LW_AT_ERROR;
        }
        return cmd->exe(at->cfg.cmd_ctx);
    }

    /* 命令名到首个 '=' 或 '?' 为止 */
    p = rest;
    while ((*p != '\0') && (*p != '=') && (*p != '?')) {
        p++;
    }
    delim = *p;
    *p = '\0';
    cmd = lw_at_cmd_dict_find(rest);
    if (cmd == NULL) {
        return LW_AT_ERROR;
    }

    /* 执行命令：AT+CMD */
    if (delim == '\0') {
        if (cmd->exe == NULL) {
            return LW_AT_ERROR;
        }
        return cmd->exe(at->cfg.cmd_ctx);
    }

    /* 查询命令：AT+CMD?，'?' 必须位于行尾 */
    if (delim == '?') {
        if (p[1] != '\0') {
            return LW_AT_ERROR;
        }
        if (cmd->query == NULL) {
            return LW_AT_ERROR;
        }
        return cmd->query(at->cfg.cmd_ctx);
    }

    /* 测试命令：AT+CMD=? */
    if ((p[1] == '?') && (p[2] == '\0')) {
        if (cmd->test == NULL) {
            return LW_AT_ERROR;
        }
        return cmd->test(at->cfg.cmd_ctx);
    }

    /* 设置命令：AT+CMD=<…> */
    if (cmd->setup == NULL) {
        return LW_AT_ERROR;
    }
    if (core_split_slots(at, &p[1]) != LW_AT_ERR_OK) {
        return LW_AT_ERROR;
    }
    return cmd->setup(at->arg_num, at->cfg.cmd_ctx);
}

/**
 * @brief 计算从 tail 到 mark 的可读字节数（环形）
 * @param stream 接收流
 * @param mark   分界位置（通常为退出时的 head 快照）
 * @return 字节数；mark 与 tail 重合时为 0
 */
static uint32_t core_bytes_to_mark(const lw_at_stream_t *stream, uint32_t mark)
{
    uint32_t tail = stream->tail;

    if (mark == tail) {
        return 0U;
    }
    if (mark > tail) {
        return mark - tail;
    }
    return (stream->size - tail) + mark;
}

/**
 * @brief 把 stream 中 [tail, mark) 交给 on_chunk，保留 mark 之后的命令字节
 * @param at   库实例
 * @param mark 数据终点（不含）
 * @return 实际交付字节数
 */
static uint32_t core_drain_to_mark(lw_at_t *at, uint32_t mark)
{
    lw_at_stream_t *stream = &at->stream;
    lw_at_data_chunk_cb_t on_chunk = at->on_chunk;
    void *user = at->data_user;
    uint32_t drained = 0U;

    (void)lw_at_stream_overflow_take(stream);
    while (stream->tail != mark) {
        const uint8_t *data;
        uint32_t n;
        uint32_t remain = core_bytes_to_mark(stream, mark);

        if (remain == 0U) {
            break;
        }
        n = lw_at_stream_peek(stream, &data);
        if (n == 0U) {
            break;
        }
        if (n > remain) {
            n = remain;
        }
        if (on_chunk != NULL) {
            on_chunk(data, n, user);
        }
        lw_at_stream_consume(stream, n);
        drained += n;
    }
    return drained;
}

/**
 * @brief 切回命令模式并清理数据会话公共状态
 * @param at            库实例
 * @param keep_pending  非 0 时若环内仍有字节则置 pending
 */
static void core_data_to_command(lw_at_t *at, uint8_t keep_pending)
{
    lw_at_transmit_reset(&at->transmit);
    at->exit_req = 0U;
    (void)lw_at_stream_overflow_take(&at->stream);
    if ((keep_pending != 0U) && (at->stream.head != at->stream.tail)) {
        at->stream.pending = 1U;
    } else {
        at->stream.pending = 0U;
    }
    at->mode = (uint8_t)LW_AT_MODE_COMMAND;
}

/**
 * @brief 完成带 exit_mark 的退出：排空分界前数据后回命令模式
 * @param at        库实例
 * @param call_done 非 0 且为 FIXED 时调用 on_done 并回 ERROR
 */
static void core_data_leave_marked(lw_at_t *at, uint8_t call_done)
{
    uint32_t drained = core_drain_to_mark(at, at->exit_mark);

    if (at->data_policy == (uint8_t)LW_AT_DATA_FIXED) {
        at->data_got += drained;
    }
    /*
     * 流式 +++ 退出保持 pending=0，需再经空闲才能解析后续命令（既有契约）。
     * 定长中止则保留环内残留，便于同轮继续处理。
     */
    core_data_to_command(at, (at->data_policy == (uint8_t)LW_AT_DATA_FIXED) ? 1U
                                                                            : 0U);
    if ((call_done != 0U) && (at->data_policy == (uint8_t)LW_AT_DATA_FIXED)) {
        if (at->on_done != NULL) {
            at->on_done(at->data_got, at->data_user);
        }
        /* 定长未收满即退出：统一回 ERROR */
        core_write_final(at, LW_AT_ERROR);
    }
}

/**
 * @brief 定长窗口：交付至多剩余字节；收满则回命令模式、调 on_done 并回 OK
 * @param at 库实例
 */
static void core_data_process_fixed(lw_at_t *at)
{
    (void)lw_at_stream_overflow_take(&at->stream);
    while (at->data_got < at->data_length) {
        const uint8_t *data;
        uint32_t n;
        uint32_t remain = at->data_length - at->data_got;

        n = lw_at_stream_peek(&at->stream, &data);
        if (n == 0U) {
            break;
        }
        if (n > remain) {
            n = remain;
        }
        if (at->on_chunk != NULL) {
            at->on_chunk(data, n, at->data_user);
        }
        lw_at_stream_consume(&at->stream, n);
        at->data_got += n;
    }
    if (at->data_got >= at->data_length) {
        core_data_to_command(at, 1U);
        if (at->on_done != NULL) {
            at->on_done(at->data_got, at->data_user);
        }
        core_write_final(at, LW_AT_OK);
    }
}

/**
 * @brief 流式：把 stream 中数据全部交给 on_chunk
 * @param at 库实例
 */
static void core_data_process_stream(lw_at_t *at)
{
    const uint8_t *data;
    uint32_t n;

    (void)lw_at_stream_overflow_take(&at->stream);
    if (at->on_chunk == NULL) {
        lw_at_stream_reset(&at->stream);
        return;
    }
    while ((n = lw_at_stream_peek(&at->stream, &data)) > 0U) {
        at->on_chunk(data, n, at->data_user);
        lw_at_stream_consume(&at->stream, n);
    }
}

/**
 * @brief 执行一行命令：解析、回最终结果行
 *
 * 有未确认的数据模式进入请求（enter_req=1）时拒绝新命令。
 * handler 内同步调 lw_at_data_confirm 后 mode 已为 DATA，本函数在
 * core_write_final 之后补打 >\r\n。异步场景 enter_req 保留待外部确认。
 * @param at   库实例
 * @param line NUL 结尾的行正文
 * @param len  行正文长度，为 0 时（空行）静默忽略
 */
static void core_exec_line(lw_at_t *at, char *line, uint32_t len)
{
    /* handler 执行结果，决定最终回包形态 */
    at_rc_t rc;

    if (len == 0U) {
        return;
    }
    /* 有未确认的数据模式入口：拒绝后续命令 */
    if (at->enter_req != 0U) {
        core_write_final(at, LW_AT_ERROR);
        return;
    }
    /* 标记在 handler 上下文内，供 lw_at_data_confirm 区分同步/异步 */
    core_in_handler = 1U;
    rc = core_parse_line(at, line);
    core_in_handler = 0U;

    /* 参数指针指向 line_buf，handler 返回后即失效 */
    at->arg_num = 0U;
    core_write_final(at, rc);

    /* handler 内同步 confirm：mode 已切 DATA，补打 >\r\n */
    if (at->mode == (uint8_t)LW_AT_MODE_DATA) {
        core_write_all(at, (const uint8_t *)core_input_prompt,
                       sizeof(core_input_prompt) - 1U);
        return;
    }
    /* 异步：保留 enter_req 门闸；ERROR 时自动清除 */
    if (at->enter_req != 0U) {
        if ((rc != LW_AT_OK) && (rc != LW_AT_RESULT_DONE)) {
            at->enter_req = 0U;
        }
    }
}

/**
 * @brief 初始化库（不做任何动态内存分配）
 */
lw_at_err_t lw_at_init(const lw_at_config_t *cfg)
{
    /* 指向本文件静态单例，后续字段全部经此写入 */
    lw_at_t *at = &at_instance;

    if (at->inited == 1U) {
        return LW_AT_ERR_STATE;
    }
    if ((cfg == NULL) ||
        (cfg->rx_buf == NULL) || (cfg->rx_buf_size < LW_AT_STREAM_SIZE_MIN) ||
        (cfg->line_buf == NULL) || (cfg->line_buf_size == 0U) ||
        (cfg->tx_buf == NULL) || (cfg->tx_buf_size == 0U) ||
        (cfg->port.write == NULL) || (cfg->port.timer_arm == NULL) ||
        (cfg->port.timer_stop == NULL) || (cfg->idle_timeout_ms == 0U)) {
        return LW_AT_ERR_PARAM;
    }

    lw_at_memset(at, 0, sizeof(*at));
    at->cfg = *cfg;
    lw_at_stream_init(&at->stream, cfg->rx_buf, cfg->rx_buf_size);
    lw_at_transmit_init(&at->transmit, cfg->guard_ms);
    lw_at_cmd_dict_reset();
    at->inited = 1U;
    return LW_AT_ERR_OK;
}

/**
 * @brief 反初始化：复位全部内部状态
 */
void lw_at_deinit(void)
{
    lw_at_cmd_dict_reset();
    if (at_instance.inited != 0U) {
        /* timer_stop 在 memset 前调用，确保 port 指针仍有效 */
        at_instance.cfg.port.timer_stop();
    }
    lw_at_memset(&at_instance, 0, sizeof(at_instance));
}

/**
 * @brief 注册一张静态命令表（转调字典模块）
 */
lw_at_err_t lw_at_cmd_register(lw_at_cmd_table_t *table)
{
    if (at_instance.inited == 0U) {
        return LW_AT_ERR_STATE;
    }
    return lw_at_cmd_dict_register(table);
}

/**
 * @brief 软件定时器到期回调（注册给 port.timer_arm）
 *
 * 可能在 ISR 上下文中执行。仅置标志不做解析或 write。
 * 命令模式下置 pending 并通知用户；数据模式下置 silent 并在
 * +++ 已凑齐时登记退出。
 * @param user 未使用
 */
static void at_timer_expired(void *user)
{
    lw_at_t *at = &at_instance;
    (void)user;

    if (at->mode == (uint8_t)LW_AT_MODE_COMMAND) {
        /* 命令模式：定时器到期 = 空闲 */
        at->stream.pending = 1U;
        if (at->cfg.cbs.idle_cb != NULL) {
            at->cfg.cbs.idle_cb(at->cfg.cbs.idle_user);
        }
    } else {
        /* 数据模式：定时器到期 = guard 静默满足 */
        uint8_t use_plus =
            (at->data_policy == (uint8_t)LW_AT_DATA_STREAM) ||
            (at->allow_plus_abort != 0U);

        if (use_plus != 0U) {
            at->silent = 1U;
            if (at->transmit.plus_cnt == LW_AT_PLUS_SEQ_LEN) {
                at->exit_mark = at->stream.head;
                at->exit_req  = 1U;
                at->transmit.plus_cnt = 0U;
            }
        }
    }
}

/**
 * @brief 唯一字节入口：入缓存并重载空闲/静默定时器，允许 ISR 调用
 */
int32_t lw_at_feed(const uint8_t *data, uint32_t len)
{
    lw_at_t *at = &at_instance;
    /* 实际接受字节数（透传路径含暂存的 '+'） */
    int32_t written;
    /* 重载定时器用时长（ms） */
    uint32_t arm_ms;

    if (at->inited == 0U) {
        return (int32_t)LW_AT_ERR_STATE;
    }
    if ((data == NULL) && (len > 0U)) {
        return (int32_t)LW_AT_ERR_PARAM;
    }
    /* 空喂数不重载定时器，避免周期性空调用饿死命令处理 */
    if (len == 0U) {
        return 0;
    }

    if (at->mode == (uint8_t)LW_AT_MODE_DATA) {
        uint8_t use_plus =
            (at->data_policy == (uint8_t)LW_AT_DATA_STREAM) ||
            (at->allow_plus_abort != 0U);

        if (at->exit_req != 0U) {
            /* 已待退出：后续字节一律按命令数据入环，模式由主路径切换 */
            written = lw_at_stream_feed(&at->stream, data, len);
        } else if (use_plus != 0U) {
            uint8_t exited = 0U;

            /* silent 标志替代原 lw_at_stream_gap() 的时间轮询 */
            written = lw_at_transmit_feed(&at->transmit, &at->stream,
                                          data, len, at->silent, &exited);
            if (exited == 1U) {
                /* ISR 只登记退出：记下分界后再写入本段命令字节 */
                at->exit_mark = at->stream.head;
                at->exit_req  = 1U;
                written = lw_at_stream_feed(&at->stream, data, len);
            }
        } else {
            written = lw_at_stream_feed(&at->stream, data, len);
        }
    } else
    {
        written = lw_at_stream_feed(&at->stream, data, len);
    }

    /*
     * 每次 feed 后清零 silent 并重载单次定时器。
     * 命令模式 arm idle_timeout_ms（空闲门限），
     * 数据模式 arm data_guard_ms（+++ 前后静默）。
     */
    if (at->mode == (uint8_t)LW_AT_MODE_DATA) {
        arm_ms = at->data_guard_ms;
    } else
    {
        arm_ms = at->cfg.idle_timeout_ms;
    }

    at->silent = 0U;
    if (arm_ms > 0U) {
        (void)at->cfg.port.timer_arm(arm_ms, at_timer_expired, NULL);
    } else {
        /* guard_ms 为 0 时无守卫：silent 立即满足 */
        at->silent = 1U;
    }

    return written;
}

/**
 * @brief 处理接收缓存：命令模式拆行分发，数据模式交付回调
 */
void lw_at_process(void)
{
    lw_at_t *at = &at_instance;
    /* get_line 返回值：行长 / NO_LINE / LINE_LONG */
    int32_t n;
    /* 指向配置中的行组装区，取行结果原地写入 */
    char *line;

    if (at->inited == 0U) {
        return;
    }
    if (at->mode == (uint8_t)LW_AT_MODE_DATA) {
        if (at->exit_req != 0U) {
            core_data_leave_marked(at, 1U);
            /* 切回命令模式后走下方 pending 门闸，不在此轮强行拆行 */
        } else if (at->data_policy == (uint8_t)LW_AT_DATA_FIXED) {
            core_data_process_fixed(at);
            if (at->mode == (uint8_t)LW_AT_MODE_DATA) {
                return;
            }
            /* 定长收满后若环内仍有命令字节，继续走 pending */
        } else {
            core_data_process_stream(at);
            return;
        }
    }
    if (lw_at_stream_pending_take(&at->stream) == 0U) {
        return;
    }

    /* 缓存满：回 ERROR 并丢弃整段缓存，即使其中已有完整行 */
    if (lw_at_stream_overflow_take(&at->stream) == 1U) {
        lw_at_stream_reset(&at->stream);
        core_write_final(at, LW_AT_ERROR);
        return;
    }

    line = (char *)at->cfg.line_buf;
    while ((n = lw_at_stream_get_line(&at->stream, line, at->cfg.line_buf_size)) !=
           LW_AT_STREAM_NO_LINE) {
        if (n == LW_AT_STREAM_LINE_LONG) {
            core_write_final(at, LW_AT_ERROR);
            continue;
        }
        core_exec_line(at, line, (uint32_t)n);

        /* 已切入数据模式：剩余缓存已作废，停止按行解析 */
        if (at->mode == (uint8_t)LW_AT_MODE_DATA) {
            break;
        }
    }
}

/**
 * @brief 读取 setup 第 index 个槽的原始正文
 */
const char *lw_at_arg_get(uint8_t index)
{
    lw_at_t *at = &at_instance;

    if (index >= at->arg_num) {
        return NULL;
    }
    return at->args[index];
}

/**
 * @brief 按字符串读取 setup 第 index 个槽
 */
lw_at_para_rc_t lw_at_get_para_str(uint8_t index, const char **out)
{
    const char *s;

    if (out == NULL) {
        return LW_AT_PARA_FAIL;
    }
    s = lw_at_arg_get(index);
    if (s == NULL) {
        return LW_AT_PARA_FAIL;
    }
    *out = s;
    if (s[0] == '\0') {
        return LW_AT_PARA_OMITTED;
    }
    return LW_AT_PARA_OK;
}

/**
 * @brief 按十进制整型读取 setup 第 index 个槽
 */
lw_at_para_rc_t lw_at_get_para_digit(uint8_t index, int32_t *out)
{
    const char *s;
    /* strtol 解析结束位置，用于确认整段均为数字 */
    char *end = NULL;
    long val;

    if (out == NULL) {
        return LW_AT_PARA_FAIL;
    }
    s = lw_at_arg_get(index);
    if (s == NULL) {
        return LW_AT_PARA_FAIL;
    }
    if (s[0] == '\0') {
        return LW_AT_PARA_OMITTED;
    }
    val = lw_at_strtol(s, &end, LW_AT_PARA_DEC_BASE);
    if ((end == NULL) || (end == s) || (*end != '\0')) {
        return LW_AT_PARA_FAIL;
    }
    if ((val < (long)INT32_MIN) || (val > (long)INT32_MAX)) {
        return LW_AT_PARA_FAIL;
    }
    *out = (int32_t)val;
    return LW_AT_PARA_OK;
}

/**
 * @brief 发送一行中间信息，自动包裹为 \r\n<正文>\r\n
 */
int32_t lw_at_send_line(const char *fmt, ...)
{
    lw_at_t *at = &at_instance;
    lw_at_va_list ap;
    /* vsnprintf 返回的正文字节数；负值或越界视为格式化失败 */
    int n;

    if ((at->inited == 0U) || (fmt == NULL)) {
        return (int32_t)LW_AT_ERR_PARAM;
    }
    lw_at_va_start(ap, fmt);
    n = lw_at_vsnprintf((char *)at->cfg.tx_buf, at->cfg.tx_buf_size, fmt, ap);
    lw_at_va_end(ap);
    if ((n < 0) || ((uint32_t)n >= at->cfg.tx_buf_size)) {
        return (int32_t)LW_AT_ERR_PARAM;
    }
    core_write_all(at, (const uint8_t *)core_crlf, sizeof(core_crlf) - 1U);
    core_write_all(at, at->cfg.tx_buf, (uint32_t)n);
    core_write_all(at, (const uint8_t *)core_crlf, sizeof(core_crlf) - 1U);
    return (int32_t)n;
}

/**
 * @brief 向主机写原始字节
 */
int32_t lw_at_write_raw(const uint8_t *data, uint32_t len)
{
    lw_at_t *at = &at_instance;

    if (at->inited == 0U) {
        return (int32_t)LW_AT_ERR_STATE;
    }
    if ((data == NULL) && (len > 0U)) {
        return (int32_t)LW_AT_ERR_PARAM;
    }
    if (len == 0U) {
        return 0;
    }
    core_write_all(at, data, len);
    return (int32_t)len;
}

/**
 * @brief 请求进入数据模式（仅登记，切换由命令执行路径完成）
 */
lw_at_err_t lw_at_data_enter(const lw_at_data_enter_cfg_t *cfg)
{
    lw_at_t *at = &at_instance;
    lw_at_data_chunk_cb_t on_chunk;
    uint32_t guard_ms;

    if (at->inited == 0U) {
        return LW_AT_ERR_STATE;
    }
    if (at->mode != (uint8_t)LW_AT_MODE_COMMAND) {
        return LW_AT_ERR_STATE;
    }
    if (cfg == NULL) {
        return LW_AT_ERR_PARAM;
    }

    on_chunk = cfg->on_chunk;
    guard_ms = cfg->guard_ms;
    if (guard_ms == 0U) {
        guard_ms = at->cfg.guard_ms;
    }

    if (cfg->policy == LW_AT_DATA_STREAM) {
        if (on_chunk == NULL) {
            return LW_AT_ERR_PARAM;
        }
    } else if (cfg->policy == LW_AT_DATA_FIXED) {
        if (cfg->length == 0U) {
            return LW_AT_ERR_PARAM;
        }
    } else {
        return LW_AT_ERR_PARAM;
    }

    at->data_policy = (uint8_t)cfg->policy;
    at->data_length = cfg->length;
    at->data_guard_ms = guard_ms;
    at->allow_plus_abort = cfg->allow_plus_abort;
    at->on_chunk = on_chunk;
    at->on_done = cfg->on_done;
    at->data_user = cfg->user;
    at->enter_req = 1U;
    return LW_AT_ERR_OK;
}

/**
 * @brief 确认进入数据模式并切入
 *
 * handler 内同步调用（core_in_handler=1）时只切模式不打印 >，
 * > 由 core_exec_line 在 \r\nOK\r\n 之后补打。外部异步调用
 * （core_in_handler=0）时自行打印 >\r\n。
 * @return LW_AT_ERR_OK 成功；LW_AT_ERR_STATE 未初始化、非命令模式
 *         或 enter_req 为 0
 */
lw_at_err_t lw_at_data_confirm(void)
{
    lw_at_t *at = &at_instance;
    /* 外部异步调用时需自行打印提示符 */
    uint8_t need_prompt;

    if (at->inited == 0U) {
        return LW_AT_ERR_STATE;
    }
    if (at->mode != (uint8_t)LW_AT_MODE_COMMAND) {
        return LW_AT_ERR_STATE;
    }
    if (at->enter_req == 0U) {
        return LW_AT_ERR_STATE;
    }
    need_prompt = (core_in_handler == 0U);
    at->enter_req = 0U;

    lw_at_stream_reset(&at->stream);
    lw_at_transmit_init(&at->transmit, at->data_guard_ms);
    at->data_got  = 0U;
    at->exit_req  = 0U;
    at->silent    = (at->data_guard_ms == 0U) ? 1U : 0U;
    at->mode      = (uint8_t)LW_AT_MODE_DATA;

    /* 数据模式 arm guard_ms；reload 语义自动取消仍在跑的命令模式空闲定时器 */
    if (at->data_guard_ms > 0U) {
        (void)at->cfg.port.timer_arm(at->data_guard_ms, at_timer_expired, NULL);
    }

    if (need_prompt) {
        core_write_all(at, (const uint8_t *)core_input_prompt,
                       sizeof(core_input_prompt) - 1U);
    }
    return LW_AT_ERR_OK;
}

/**
 * @brief 取消待确认的数据模式进入请求
 */
lw_at_err_t lw_at_data_cancel(void)
{
    lw_at_t *at = &at_instance;

    if (at->inited == 0U) {
        return LW_AT_ERR_STATE;
    }
    if (at->enter_req == 0U) {
        return LW_AT_ERR_STATE;
    }
    at->enter_req = 0U;
    return LW_AT_ERR_OK;
}

/**
 * @brief 应用主动退出数据模式
 */
lw_at_err_t lw_at_data_exit(void)
{
    lw_at_t *at = &at_instance;

    if (at->inited == 0U) {
        return LW_AT_ERR_STATE;
    }
    if (at->mode != (uint8_t)LW_AT_MODE_DATA) {
        return LW_AT_ERR_STATE;
    }
    at->exit_mark = at->stream.head;
    at->exit_req = 1U;
    core_data_leave_marked(at, 1U);
    return LW_AT_ERR_OK;
}

/**
 * @brief 请求进入流式透传（兼容封装）
 */
lw_at_err_t lw_at_transmit_enter(void)
{
    lw_at_t *at = &at_instance;
    lw_at_data_enter_cfg_t cfg;

    if (at->inited == 0U) {
        return LW_AT_ERR_STATE;
    }
    if ((at->mode != (uint8_t)LW_AT_MODE_COMMAND) || (at->cfg.cbs.sink == NULL)) {
        return LW_AT_ERR_STATE;
    }
    lw_at_memset(&cfg, 0, sizeof(cfg));
    cfg.policy = LW_AT_DATA_STREAM;
    cfg.on_chunk = at->cfg.cbs.sink;
    cfg.user = at->cfg.cbs.sink_user;
    cfg.guard_ms = at->cfg.guard_ms;
    return lw_at_data_enter(&cfg);
}
