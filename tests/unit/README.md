# 模块测试（unit）

本目录验证单个源文件或窄边界职责，改对应模块时优先在这里跑。

| 子目录 | 被测模块 |
| --- | --- |
| [lw_at_stream](lw_at_stream/TEST.md) | `lw_at_stream.c` |
| [lw_at_cmd_dict](lw_at_cmd_dict/TEST.md) | `lw_at_cmd_dict.c` |
| [lw_at_transmit](lw_at_transmit/TEST.md) | `lw_at_transmit.c` |
| [lw_at_core](lw_at_core/TEST.md) | `lw_at_core.c`（经对外 API；含流式/定长数据模式） |

总览与选型见 [../README.md](../README.md)。系统级场景见 [../system](../system)。
