# LW-AT移植说明


---

| 日期 | 作者 | 说明 |
| :---: | :---: | --- |
| 2026.08.01 | linzhiwei | 移植说明初版 |
| | | |


## 移植方式:
所需移植的文件内容位于`/src`文件夹内

```c
── src/                                # 框架核心源码目录
│   ├── lw_at.h                        # 对外 API（应用层仅需包含此文件）
│   ├── lw_at_port.h                   # 移植层：配置宏、C 库映射与定时器回调类型
│   ├── lw_at_core.h / lw_at_core.c
│   ├── lw_at_stream.h / lw_at_stream.c
│   ├── lw_at_transmit.h / lw_at_transmit.c
│   └── lw_at_cmd_dict.h / lw_at_cmd_dict.c
```

移植方需实现板级适配 `lw_at_port_ops_t`：

```c
typedef struct {
    int32_t (*write)(const uint8_t *data, int32_t len);      /* 必填；data 只读 */
    int32_t (*timer_arm)(uint32_t ms, lw_at_timer_cb_t cb, void *user); /* 必填；启动/重载单次软件定时器 */
    void    (*timer_stop)(void);                              /* 必填；停止定时器 */
} lw_at_port_ops_t;
```

- `write`：向主机发送应答与中间信息；短写时应尽量写完，失败按传输故障处理。
- `timer_arm`：启动/重载单次软件定时器（重复调用覆盖前次）；到期时由板级在串行上下文触发 `cb`。命令模式 arm 空闲门限 `idle_timeout_ms`，数据模式 arm `guard_ms`。
- `timer_stop`：停止定时器（`lw_at_deinit` 时调用）。

空闲与数据模式静默的计时完全依赖该单次软件定时器：库不做时间轮询。

## 内存与配置

库不使用动态内存。`lw_at_config_t` 中 rx_buf / line_buf / tx_buf 均由调用方静态提供，需先实例化存储区：

```c
/* 存储区由调用方静态定义，大小按产品吞吐量调整 */
static uint8_t rx_mem[256];   /* 接收环形缓存：实际可用容量为 size-1，最小 2 字节 */
static uint8_t line_mem[64];  /* 单行命令组装区：决定最大命令行长（含结尾 NUL） */
static uint8_t tx_mem[64];    /* lw_at_send_line 格式化区 */

/* 板级适配：write 与单次软件定时器 */
static int32_t my_write(const uint8_t *data, int32_t len);
static int32_t my_timer_arm(uint32_t ms, lw_at_timer_cb_t cb, void *user);
static void    my_timer_stop(void);

lw_at_config_t cfg;
cfg.rx_buf = rx_mem;  
cfg.rx_buf_size = sizeof(rx_mem);
cfg.line_buf = line_mem; 
cfg.line_buf_size = sizeof(line_mem);
cfg.tx_buf = tx_mem;  
cfg.tx_buf_size = sizeof(tx_mem);
cfg.port.write = my_write;
cfg.port.timer_arm = my_timer_arm;
cfg.port.timer_stop = my_timer_stop;
cfg.idle_timeout_ms = 50;
cfg.guard_ms = 100;
if (lw_at_init(&cfg) != LW_AT_ERR_OK) { /* 失败处理 */ }
```

要点：
- 缓冲区必须为 `uint8_t` 数组且生命周期覆盖整个使用期；`lw_at_init` 只保存指针，不拷贝。
- `rx_buf_size` 须 ≥ 2；`line_buf_size` / `tx_buf_size` 须 > 0。
- 全部缓冲区须在 `lw_at_init` 前就绪（`lw_at_config_t` 中相关字段为必填）。

## 参考实现

- Windows 移植参考：`port/lw_at_port_win.c` / `.h`（控制台 write + Windows 线程池定时器）。
- 最小接入示例：`examples/usage/`（MCU 风格主循环）。

## 调用方式
当平台适配完成后即可使用，应用层仅需包含 `lw_at.h`。
