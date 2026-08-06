# 系统测试（system）

本目录验证整库在典型用法与场景下的行为，而不是单个源文件的内部算法。

| 子目录 | 验证重点 |
| --- | --- |
| [host](host/TEST.md) | 单线程主路径快速回归 |
| [async_script](async_script/TEST.md) | 从机内部 feed / 定时器回调 / process 时序 |
| [host_slave_win](host_slave_win/TEST.md) | 主机↔从机双线程通测（多命令、压力、损伤） |
| [host_slave_send](host_slave_send/TEST.md) | CIPMODE + CIPSEND 专题（定长/流式模式切换） |

总览与选型见 [../README.md](../README.md)。模块级测试见 [../unit](../unit)。
