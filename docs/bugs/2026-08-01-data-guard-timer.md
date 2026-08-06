# Bug 记录：数据模式定时器未重载导致 `+++` 静默守卫失效

**状态**：✅ 已修复（2026-08-01，`lw_at_core.c` v1.10.1）
**发现日期**：2026-08-01（Bugbot 审查发现 + 人工核实）
**严重度**：高（同步场景 silent 提前置 1） / 中（异步场景 silent 恒为 0）
**位置**：`lw_at/src/lw_at_core.c` — `lw_at_data_confirm()`（约 L935-965）

---

## 问题描述

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

---

## 根因

`lw_at_data_confirm` 缺少 `timer_arm(at->data_guard_ms, at_timer_expired, NULL)` 调用，
以装载数据模式静默定时器并取消仍在运行的旧空闲定时器。

`lw_at_feed` 末尾虽然会根据当前模式 arm 定时器，但 confirm 切 DATA 后到下一笔
feed 之间有一段"无定时器运行"的窗口。

---

## 复现条件（例程未覆盖的原因）

例程在 `lw_at_data_confirm` 之后总是先发 `ping` / `hello`（DATA 模式第一笔 feed），
该 feed 会以正确的 `data_guard_ms` 重装定时器，因此未触发 bug。

若 confirm 后直接 `Sleep(guard_ms)` 再发 `+++`（中间无任何 feed），即可复现：
主机心想"已等够 guard_ms"，发 `+++` 期望退出，但库这边根本没在计时，
`silent` 恒为 0，`+++` 被当作普通 payload 交给 `on_chunk`，退出失败。

---

## 修复方案（已实施）

在 `lw_at_data_confirm()` 切到 DATA 模式后（`at->mode = LW_AT_MODE_DATA;` 之后）加：

```c
/* 数据模式 arm guard_ms；timer_arm reload 语义自动取消仍在跑的 idle 定时器 */
if (at->data_guard_ms > 0U) {
    (void)at->cfg.port.timer_arm(at->data_guard_ms, at_timer_expired, NULL);
}
```

### 修复覆盖

| 发现 | 修复作用 |
| --- | --- |
| 同步场景 `silent` 提前置 1 | reload 自动取消旧 idle 定时器，`silent` 不再被提前置 1 |
| 异步场景 `silent` 恒为 0 | 重新计时 `guard_ms`，到期置 `silent=1`，主机等待后发 `+++` 可被识别 |

### 验证

- `unit/lw_at_core` C13 透传退出加固用例全数通过（含此前失败的空 feed / 退出排空场景）。
- **新增回归用例**（C13 末尾）：进入流式后不发任何数据，静默 `guard_ms` 后直接发 `+++` 应能退出。
  负向验证：临时禁用 confirm 中的 arm 代码后，该用例 3 项断言失败（`sink` 收到 `+++` 无法退出）；
  恢复后通过。该用例永久锁定「confirm 须用 guard_ms 重载定时器」这一契约。

---

## 参考代码路径

- `lw_at_data_confirm()`：`lw_at/src/lw_at_core.c` L935-965
- `lw_at_feed()` 末尾定时器装载：`lw_at/src/lw_at_core.c` L683-699
- `at_timer_expired()` DATA 分支：`lw_at/src/lw_at_core.c` L608-621
- `lw_at_transmit_feed()` silent 判定：`lw_at/src/lw_at_transmit.c` L104-113
