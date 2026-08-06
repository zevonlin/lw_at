## LW-AT简介
---

## 设计理念：
lw-at 是面向裸机环境的 AT 从机解析库，将设备侧固件变成 AT 交互的通信模组：主机经 UART/IIC/SPI 下发 AT 命令，库完成收字节、组帧/拆帧、命令查表执行与结果回包。

设计上遵循以下理念：

- **事件驱动计时**：空闲与透传静默依赖板级单次软件定时器（timer_arm/timer_stop），定时器到期回调内库只置标志，不做解析或 write；无需任何时间轮询。
- **零动态内存**：接收缓存、行组装区、发送格式化区全部由调用方静态提供，库内不调用任何内存分配。
- **两阶段数据模式确认**：进入数据模式先回 OK，主机收到 `>` 提示符后再发数据；流式透传与定长收数共用同一数据模式框架。
- **单实例、串行模型**：库为单实例且不具备线程安全，所有处理入口在串行上下文调用，`lw_at_feed` 例外允许 ISR。
- **纯 C、易移植**：仅依赖标准 C 库函数（经 lw_at_port.h 宏映射可替换），任意 C 语言环境可移植。

具体 API 说明见 [docs/core_api.md](docs/core_api.md)，接入步骤见 [docs/porting_guide.md](docs/porting_guide.md)。

## 设备框架：
该框架为通用框架，允许移植在任何 C 语言开发环境中。移植方需实现 lw_at_port_ops_t（write / timer_arm / timer_stop）并提供静态缓冲区。

## 文件框架:
```c
lw-at/
├── README.md                               # 项目简介
├── ChangeLog.md                            # 修改日志
│
├── LICENSE                                 # 开源协议
│
├── docs/                                   # 文档目录（根据维护情况可能为空）
│   ├── png/                                # 文档图片
│   ├── porting_guide.md                    # 移植指南
│   ├── core_api.md                         # API 参考文档
│   └── lw_at_readme.md                     # LW-AT 需求文档（基本需求说明）
│
├── src/                                    # 框架核心源码目录
│   ├── lw_at.h                             # 对外 API（应用层仅需包含此文件）
│   ├── lw_at_port.h                        # 移植层：配置宏、C 库映射与定时器回调类型
│   ├── lw_at_core.h / lw_at_core.c         # 核心：生命周期、模式、行解析与回包
│   ├── lw_at_stream.h / lw_at_stream.c     # 接收流：环形缓存、满丢弃、取行
│   ├── lw_at_transmit.h / lw_at_transmit.c # 流式透传与 +++ 静默守卫
│   └── lw_at_cmd_dict.h / lw_at_cmd_dict.c # 命令字典：注册与查找
│
├── port/                                   # 平台移植参考目录
│   ├── README.md
│   └── lw_at_port_win.h / lw_at_port_win.c # Windows 移植参考（控制台 + 线程池定时器）
│
├── examples/                               # 示例工程目录
│   └── usage/                              # 使用示例工程（Windows 主循环 + 精简命令）
│       ├── main.c
│       ├── Makefile
│       └── cmd/
│           ├── lw_at_cmd_ex.h
│           └── lw_at_cmd_ex.c
│
└── tests/                                  # 回归测试（unit / system / fixtures）
```

## 支持设备：
该框架为通用框架,允许移植在任何C语言开发环境中。

## 建议与注意事项：
- 单实例、非线程安全：所有 lw_at_* API 须在串行上下文中调用；lw_at_feed 允许 ISR。
- 不使用动态内存：接收缓存、行缓存、发送缓存均由调用方静态提供并传入 lw_at_config_t。
- 空闲与静默依赖板级单次软件定时器（timer_arm/timer_stop），须在板级实现并触发到期回调。
- 数据模式进入采用两阶段确认，主机须等收到 `>` 提示符后再发送数据。
- 命令帧边界仅以 \r\n 为准；空闲超时不作为帧结束条件。
- 内核不内置业务命令名（如 CIPMODE/CIPSEND），由产品侧命令表实现。

## 相关信息：
```c
@module name  :lw_at
@author       :linzhiwei(zevonlin)
@email        :zevonlin@gmail.com
@date         :2026-08-01
@note         :
@see          :https://github.com/zevonlin
@main Version :v0.9.0
```
