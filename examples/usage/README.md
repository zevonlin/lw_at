# lw_at 使用示例

本目录提供一份可在 Windows 上直接编译运行的最小接入示例，说明产品侧如何把 LW-AT 接到「主循环 + 板级适配」模型上。

## 运行

```bash
cd examples/usage
make run
```

需要本机提供 `gcc` 与 `make`。

## 内容

| 文件 | 说明 |
| --- | --- |
| `main.c` | 程序入口：init、注册命令、发送演示序列（含定长/流式数据模式）。 |
| `Makefile` | 编译脚本。 |
| `cmd/` | 例程命令表：AT / INT / STR / MIX / SLOT / CIPMODE / CIPSEND。 |

演示命令均需以 `\r\n` 结尾（定长/透传负载除外）。数据模式进入采用两阶段确认：`>` 提示符出现后再发数据。

## 相关信息

接入步骤与 Port 接口约定见仓库根目录 `README.md` 与 `docs/porting_guide.md`。
