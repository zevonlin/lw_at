# BUG-000：数据模式定时器未重载导致 `+++` 静默守卫失效

## 追踪信息

| 项 | 值 |
| --- | --- |
| Bug ID | BUG-000 |
| 标题 | 数据模式定时器未重载导致 `+++` 静默守卫失效 |
| 状态 | 已修复 |
| 优先级 | 高（同步场景 silent 提前置 1） / 中（异步场景 silent 恒为 0） |
| 修复版本 | lw_at_core.c v1.10.1 |
| Git 提交 | - |
| 发现日期 | 2026-08-01 |
| 修复日期 | 2026-08-01 |
| 发现来源 | Bugbot 审查发现 + 人工核实 |
| 修复方式 | `lw_at_data_confirm()` 切入 DATA 后用 `guard_ms` 重载单次定时器（timer_arm reload） |

## 现象

`lw_at_data_confirm()` 将 `mode` 切到 `LW_AT_MODE_DATA` 后，**没有用 `data_guard_ms` 重新装载定时器**，导致两种异常：

### 1. 同步场景（handler 内 confirm）—— `silent` 提前置 1

此前 `lw_at_feed` 装载的命令模式定时器（`idle_timeout_ms`，如 50ms）仍在运行。
它在 DATA 模式下到期时，`at_timer_expired` 走 else 分支把 `silent` 置 1，
**`+++` 静默守卫时长被错误缩短为 `idle_timeout_ms`**（而非 `data_guard_ms`，如 100ms）。

### 2. 异步场景（外部 confirm）—— `silent` 恒为 0

上一个命令模式定时器已到期烧完（一次性定时器），`lw_at_data_confirm` 切 DATA
后置 `silent=0`，但**没有任何定时器在运行**。主机等待 `guard_ms` 后发 `+++` 时，
`lw_at_transmit_feed` 因 `silent==0` 把 `+` 当普通数据处理，
**`+++` 退出序列永远无法识别**，主机卡死在透传模式。

## 根因

`lw_at_data_confirm` 缺少 `timer_arm(at->data_guard_ms, at_timer_expired, NULL)` 调用，
以装载数据模式静默定时器并取消仍在运行的旧空闲定时器。

`lw_at_feed` 末尾虽然会根据当前模式 arm 定时器，但 confirm 切 DATA 后到下一笔
feed 之间有一段"无定时器运行"的窗口。

## 协议角度分析

`+++` 退出守卫要求**前后静默时长统一按 `guard_ms` 计量**，且进入数据模式后主机即按
`guard_ms` 预期静默条件：

| 协议本意 | 当前缺陷行为 |
| --- | --- |
| 前后静默时长统一为 `guard_ms` | 同步场景被错误缩短为 `idle_timeout_ms` |
| 主机等待 `guard_ms` 后发 `+++` 应可识别退出 | 异步场景 `silent` 恒 0，`+++` 被当普通数据，永远无法退出 |
| 退出行为确定、可预期 | 同一主机用法在同步/异步确认下结果不一致 |

## 复现步骤

1. 进入 STREAM 数据模式（`AT+CIPMODE=1` + 无参 `CIPSEND` → `\r\nOK\r\n>\r\n`）。
2. confirm 后**不发任何数据**，直接等待 `guard_ms`（例程总是先发 `ping`/`hello`，
   该 feed 会以正确 `guard_ms` 重装定时器，因此未暴露）。
3. 发 `+++`，再等待 `guard_ms`。
4. 期望退出回命令模式；实际：异步场景 `silent` 恒 0，`+++` 进入 `on_chunk` 无法退出；
   同步场景 `silent` 提前置 1，守卫被错误缩短。

## 修复方向（已确定）

在 `lw_at_data_confirm()` 切到 DATA 模式后（`at->mode = LW_AT_MODE_DATA;` 之后）用
`guard_ms` 重载单次定时器，利用 timer_arm 的 reload 语义自动取消仍在运行的旧空闲定时器。

## 相关测试

- `unit/lw_at_core` C13 透传退出加固用例全数通过（含此前失败的空 feed / 退出排空场景）。
- **新增回归用例**（C13 末尾）：进入流式后不发任何数据，静默 `guard_ms` 后直接发 `+++`
  应能退出。负向验证：临时禁用 confirm 中的 arm 代码后，该用例 3 项断言失败（`sink` 收到
  `+++` 无法退出）；恢复后通过。该用例永久锁定「confirm 须用 `guard_ms` 重载定时器」契约。

## 相关文档差异

- 当时 `docs` 未明确「confirm 后须立即用 `guard_ms` 重载定时器」这一要求；实现行为以本次
  修复为准，并由 C13 回归用例锁定。
- 数据模式静默计时入口的完整约定见 `docs/porting_guide.md`（数据模式 arm `guard_ms`）。

## 修复记录（2026-08-01）

**修复内容**：`lw_at_data_confirm()` 切入 DATA 模式后立即重载静默定时器：

```c
/* 数据模式 arm guard_ms；timer_arm reload 语义自动取消仍在跑的 idle 定时器 */
if (at->data_guard_ms > 0U) {
    (void)at->cfg.port.timer_arm(at->data_guard_ms, at_timer_expired, NULL);
}
```

**修复覆盖**：

| 发现 | 修复作用 |
| --- | --- |
| 同步场景 `silent` 提前置 1 | reload 自动取消旧 idle 定时器，`silent` 不再被提前置 1 |
| 异步场景 `silent` 恒为 0 | 重新计时 `guard_ms`，到期置 `silent=1`，主机等待后发 `+++` 可被识别 |

**验证**：`unit/lw_at_core` C13 全数通过；新增回归用例锁定「confirm 后无数据、静默到期
直接 `+++` 应退出」契约。
