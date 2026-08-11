# LW-AT 测试说明

本目录存放 LW-AT 的回归与验证代码，**不属于库核心**。被测实现位于 `../src`；面向产品接入的例程位于 `../examples`。

测试按职责分成两层：

| 目录 | 含义 |
| --- | --- |
| [unit/](unit/) | **模块测试**：主要验证单个源文件或窄边界（stream / cmd_dict / transmit / core） |
| [system/](system/) | **系统测试**：验证整库用法与场景（单线程主路径、从机内部时序、主机↔从机双端） |
| [fixtures/](fixtures/) | 测试辅助：`test_port`（虚拟单次定时器与应答捕获）、`test_cmd`（`AT` / `ECHO` / `CIPMODE` / `CIPSEND`） |

在任意测试子目录中执行：

```bash
make run
```

依赖本机 `gcc` 与 `make`。其中 `system/host_slave_win` 与 `system/host_slave_send` 依赖 Windows 线程 API，仅在 Windows 上构建。进程退出码为 0 表示该目录全部检查通过。

## 推荐跑法

1. **改了某一内部模块**：先跑 `unit/` 下对应目录（`lw_at_stream`、`lw_at_cmd_dict`、`lw_at_transmit` 或 `lw_at_core`）。
2. **小改动后确认主路径仍可用**：跑 `system/host`。
3. **变更涉及空闲超时、分片到达、多行粘连、不完整行超时作废等从机内部时序**：跑 `system/async_script`。
4. **变更涉及 CIPMODE / CIPSEND（定长或流式）、模式进出、粘连丢载荷、应用退出数据模式**：优先跑 `system/host_slave_send`；单线程定长/流式基线见 `unit/lw_at_core`（C09–C14）。
5. **发版前，或变更涉及主机连发、应答收集、链路分片/丢字节、通用双端异步**：跑 `system/host_slave_win`。

## unit/：模块测试

| 目录 | 验证对象 | 建议使用场景 |
| --- | --- | --- |
| [lw_at_stream](unit/lw_at_stream/TEST.md) | 接收环形缓冲区、仅以 `\r\n` 取行、待处理/满丢弃标志、peek/consume | 修改收包或取行实现时 |
| [lw_at_cmd_dict](unit/lw_at_cmd_dict/TEST.md) | 命令表注册、重名拒绝、按名精确查找（区分大小写） | 修改命令字典实现时 |
| [lw_at_transmit](unit/lw_at_transmit/TEST.md) | 透传数据写入与转发、退出序列 `+++` 的识别与前后静默约束 | 修改透传/流式守卫实现时 |
| [lw_at_core](unit/lw_at_core/TEST.md) | 整机串联 API：命令四种形态、空闲到期后再处理、接收溢出、流式/定长数据模式、设置参数拆分矩阵 | 修改 core，或修改设置参数解析 / 数据模式回包时 |

说明：`lw_at_core` 会链接多个源文件，但仍按库对外 API 验证 core 职责，因此归在模块测试一侧。

## system/：系统测试

| 目录 | 验证对象 | 建议使用场景 |
| --- | --- | --- |
| [host](system/host/TEST.md) | 单线程集成快速检查：主路径能否正确 init、注册、应答 | 日常改动后的快速回归 |
| [async_script](system/async_script/TEST.md) | 从机内部确定时序：中断式分片喂入与主循环处理的交错 | 修改空闲语义、分片组帧或透传时序相关行为时 |
| [host_slave_win](system/host_slave_win/TEST.md) | 主机线程与从机线程经字节链路的异步交互，含压力与损伤（通测） | 发布前回归，或验证「主机不等待 OK 即连发」等双端行为时 |
| [host_slave_send](system/host_slave_send/TEST.md) | CIPMODE + CIPSEND 专题：定长/流式进出、粘连丢载荷、打断与溢出契约 | 改数据模式 API、进入提示、定长结案或 SEND 类命令行为时 |

`host_slave_win` 完整场景见 [system/host_slave_win/PLAN.md](system/host_slave_win/PLAN.md)。  
`host_slave_send` 契约与场景矩阵见 [system/host_slave_send/PLAN.md](system/host_slave_send/PLAN.md)。

## 按变更类型选择套件

| 变更类型 | 建议运行的目录 |
| --- | --- |
| 设置参数如何按逗号拆成多个槽、如何取整数/字符串、省略参数槽与引号转义 | `unit/lw_at_core`（其中 C12 为完整字符矩阵） |
| 空闲超时多久才允许 `process`、尚未结束的不完整行在超时后的作废行为 | `unit/lw_at_stream` 与 `system/async_script` |
| 流式 `+++` 识别与静默 | `unit/lw_at_transmit` 与 `unit/lw_at_core`；双端再跑 `host_slave_send` / `host_slave_win` 透传组 |
| 定长 `CIPSEND=<len>`、进入提示（`\r\nOK\r\n>\r\n`）、收满 OK / 中止 ERROR | `unit/lw_at_core`（C14）与 `system/host_slave_send` |
| 主机在未收到 OK 时继续发送后续命令 | 以 `system/host_slave_win` 为主；SEND 粘连丢载荷以 `host_slave_send` A-01/A-02 为准 |
