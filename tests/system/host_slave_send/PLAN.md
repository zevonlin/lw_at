# host_slave_send：CIPMODE + CIPSEND 专题计划

与 `host_slave_win`（双端通测）分离。本目录只验证 **命令模式 ↔ 数据模式** 围绕 `AT+CIPMODE` / `AT+CIPSEND` 的真实主从交互问题。

## 1. 双端模型

```text
主线程（主机）                         从机线程（唯一调用 lw_at_*）
  host_send / collect                    feed ← 下行队列
       |                                 tick / process
       v                                      |
  下行字节队列  --------------------------->  |
       ^                                      | Port.write
       |                                      v
  上行字节队列  <-----------------------------
```

侧信道（不经 UART）：`slave_rt_request_data_exit()` 置位，从机循环内调用 `lw_at_data_exit()`，模拟应用层主动退出数据模式（断链、超时等）。

## 2. 内核契约（测例不得“改期望绕过”）

下列行为以当前内核为准；失败时先区分 **内核缺陷** 与 **测例误解契约**：

| 契约 | 说明 |
| --- | --- |
| 进入数据模式清空接收环 | handler 成功切入时 `stream_reset`：与 `CIPSEND` **同一批已入环、行后粘连的载荷会被丢弃**。主机须等 `\r\nOK\r\n>\r\n` 后再发数据。 |
| 进入提示（OK_PROMPT 字节） | 精确字节：`\r\nOK\r\n>\r\n` |
| 定长收满 | `on_done` 后库回 `\r\nOK\r\n`，回命令模式；环内多余字节可保留（pending） |
| 定长中止 | `+++`（若 `allow_plus_abort`）或 `lw_at_data_exit`：`on_done(got)` 后回 `\r\nERROR\r\n` |
| 流式 `+++` 退出 | 排空后回命令模式并回 `\r\nOK\r\n` |
| 流式中 AT 文本 | 当普通数据进 sink，不解析 |
| CIPMODE | **应用侧变量**，非内核模式；无参 `CIPSEND` 仅当 CIPMODE=1 才请求流式 |
| 有参 `CIPSEND=<len>` | 与 CIPMODE 无关，直接定长窗口 |
| FIXED 默认无 `+++` | `allow_plus_abort=0` 时 `+++` 计入定长载荷；本套件用 `AT+CIPABORT` 打开取消路径 |
| 流式环溢出 | DATA 下 `overflow_take` 被吞掉，**不**向主机回 ERROR；仅丢弃放不下的新字节 |
| 命令模式环溢出 | process 回 `\r\nERROR\r\n` 并清空环 |

## 3. 命令表 `send_cmd`

| 命令 | 作用 |
| --- | --- |
| `AT` | 联通 / 回命令模式确认 |
| `AT+CIPMODE=<0\|1>` / `?` | 应用侧传输模式 |
| `AT+CIPABORT=<0\|1>` / `?` | 后续定长 `CIPSEND=<len>` 是否允许 `+++` 取消 |
| `AT+CIPSEND=<len>` | 定长；回进入提示；收满 OK / 中止 ERROR |
| `AT+CIPSEND` | 无参；仅 CIPMODE=1 进流式；否则 ERROR |

## 4. 场景矩阵

### 4.1 门控 G（CIPMODE / 非法 SEND）

| ID | 场景 | 期望 |
| --- | --- | --- |
| G-01 | CIPMODE=0 后无参 CIPSEND | ERROR；仍为命令模式 |
| G-02 | CIPMODE=1 后无参 CIPSEND | `\r\nOK\r\n>\r\n` |
| G-03 | CIPMODE=2 / 非法 | ERROR |
| G-04 | CIPSEND=0 / 非数字 / 负数 | ERROR |
| G-05 | CIPMODE? 反映最近合法设置 | `+CIPMODE:n` + OK |

### 4.2 定长 F（CIPSEND=\<len\>）

| ID | 场景 | 期望 |
| --- | --- | --- |
| F-01 | =5，整包 5 字节 | 提示后收满；捕获正文；主机见 OK；再 AT→OK |
| F-02 | 载荷分片 2+3 | 同上 |
| F-03 | 收满后环内粘连下一命令 `AT` | 定长 OK 后，经 idle 处理得 AT 的 OK |
| F-04 | 未收满期间发的“AT\\r\\n”计入载荷（abort=0） | 不解析；凑满后 OK |
| F-05 | CIPABORT=1 后合法 +++ 中止 | on_done 部分长度；主机 ERROR；再 AT→OK |
| F-06 | 等提示后再发（规范主机） | 成功路径基线 |

### 4.3 流式 S（无参 CIPSEND）

| ID | 场景 | 期望 |
| --- | --- | --- |
| S-01 | 进入→分片数据→合法 +++ | sink 正确；退出自动回 OK；AT→OK |
| S-02 | 前静默不足的 +++ | 作数据进 sink；再合法退出 |
| S-03 | 透传中发 AT\\r\\n | 进 sink；退出后命令恢复 |
| S-04 | 较大载荷 | 长度正确 |
| S-05 | 后静默不足再跟字节 | +++ 还原转发；再合法退出 |

### 4.4 异步 / 粘连 A（真实主机易踩坑）

| ID | 场景 | 期望 |
| --- | --- | --- |
| A-01 | 单次写入 `CIPSEND\\r\\nhello`（未等 >） | 提示发出；**hello 被 enter 清空丢弃**；补发后 sink 才有 hello |
| A-02 | 单次写入 `CIPSEND=4\\r\\n1234` | 提示发出；1234 丢弃；补发后收满 OK |
| A-03 | `CIPMODE=1\\r\\nCIPSEND\\r\\n` 一次写入 | 两行依次执行；得 OK 与进入提示 |
| A-04 | 见提示后立即发数据（几乎无间隙） | 数据应进 sink/定长窗口（正常路径） |

### 4.5 切换 X

| ID | 场景 | 期望 |
| --- | --- | --- |
| X-01 | 定长成功 → 流式 → +++ → 定长 | 三次均成功 |
| X-02 | 流式退出后 CIPMODE=0，无参 CIPSEND | ERROR |
| X-03 | 流式中不可再发 CIPSEND 当命令 | 文本进 sink |

### 4.6 打断 I（应用退出 / 取消）

| ID | 场景 | 期望 |
| --- | --- | --- |
| I-01 | 定长未满，侧信道 data_exit | ERROR；got 为已收；AT→OK |
| I-02 | 流式中侧信道 data_exit | 无 OK/ERROR；AT→OK |
| I-03 | 定长 abort=1，假 +++（无前静默） | 计入数据或按守卫规则；可再合法取消 |

### 4.7 缓冲 B

| ID | 场景 | 期望 |
| --- | --- | --- |
| B-01 | 偏小 rx，流式灌入超量 | **DATA 下 overflow 不回 ERROR**（内核吞掉标志）；sink 长度小于发送量；仍可 `+++` 退出后 `AT→OK` |
| B-CMD | 命令模式灌满接收环 | 精确 `\r\nERROR\r\n`，随后 `AT→OK` |
| B-02 | 场景复位后状态不串扰 | reinit 默认缓冲后 CIPMODE=0，`AT→OK` |

### 4.8 真实交互加固 R

| ID | 场景 | 期望 |
| --- | --- | --- |
| R-01 | `+++` 退出后立刻再进流式 | 第二次 sink 仅新数据；可再退出 |
| R-02 | 定长载荷含 `0x00` | 收满 OK；字节逐一匹配；got=len |
| R-03 | 见提示后无延时立即发定长 | 收满 OK |
| R-04 | `CIPSEND=3` 命令与载荷均字节切分 | 提示与收满均成功 |

## 5. 与 host_slave_win 的边界

| | host_slave_win | host_slave_send |
| --- | --- | --- |
| 目标 | 双端通用健壮性 | SEND / 模式切换专题 |
| 命令 | 多业务命令 | 仅 CIP* + AT |
| 定长 CIPSEND | 无 | 有 |
| 粘连丢载荷契约 | 未专项 | A-01/A-02 专测 |
| 应用 data_exit | 无 | 侧信道专测 |

## 6. 排障原则

1. 先对照 §2 契约，再怀疑内核。
2. 墙钟 + idle/guard：收集窗口须 ≥ idle + 余量；+++ 前后静默 ≥ guard。
3. 场景间 `host_scene_begin` + `send_cmd_reset` + 吸干队列。
4. 禁止为让测例通过而放宽对「粘连载荷应保留」的错误期望（除非先改内核并升版本说明）。
5. 2026-07-31：测例期望 `\r\nOK\r\n>\r\n`，内核曾误为仅写 `>`；属内核回归，已恢复 `>\r\n`，未改测例契约。
6. 加严后：enter 失败则跳过该场景后续步骤；结果码用 `host_wait_exact`；流式退出须先消费退出回包 `\r\nOK\r\n`（`host_exit_stream_ok`），再验证后续命令。
