# hft_md_gateway

一个面向高频场景的行情网关示例项目，统一封装了行情源、解码器和发布器，当前仓库主要覆盖以下链路：

- CTP 行情接入 -> DAT 文件落盘
- CTP 行情接入 -> 共享内存发布
- PCAP 文件回放 / 实时抓包 -> DAT 文件落盘
- EFVI 数据源 -> DAT 文件落盘

## 构建

依赖：

- C++17
- CMake 3.16+
- `libpcap`
- `pthread`
- CTP 行情库（仓库已包含 `third_party/ctp`）

构建命令：

```bash
cmake -S . -B build
cmake --build build -j
```

产物默认输出到根目录 `bin/`。

## 运行

各 demo 都接收一个 YAML 配置文件：

```bash
./bin/ctp_md_demo conf/ctp_dat_demo.yaml
./bin/ctp_shm_demo conf/ctp_shm_demo.yaml
./bin/pcap_md_demo conf/pcap_dat_demo.yaml
./bin/pcap_live_demo conf/pcap_live_demo.yaml
./bin/efvi_md_demo conf/efvi_dat_demo.yaml
```

如果使用共享内存发布，可用下面的工具查看输出：

```bash
./bin/shm_tick_reader CTP_MD
```

## 配置说明

`conf/` 下提供了示例配置，结构基本分为两段：

- `source`: 行情源参数，例如 CTP 前置、PCAP 文件路径、抓包过滤条件、订阅合约、交易日
- `publisher`: 输出参数，例如 DAT 落盘目录、文件后缀、共享内存名称和容量

修改前建议先确认：

- CTP 账号、密码、前置地址是否可用
- `publisher.output_path` 是否存在且可写
- PCAP 模式下 `pcap_file` 或网卡相关参数是否正确

## 目录

```text
include/    头文件
src/app/    各类 demo 入口
conf/       示例配置
tools/      调试/辅助工具
third_party/ 第三方依赖
```
