# Landscape Audit（景观审计）

[English](README.md) | [简体中文](README.zh-CN.md)

**面向离散局部搜索的精确邻域闭包与中性平台认证。**

[![CI](https://github.com/PowellWells/landscape-audit/actions/workflows/ci.yml/badge.svg)](https://github.com/PowellWells/landscape-audit/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/PowellWells/landscape-audit)](https://github.com/PowellWells/landscape-audit/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

<p align="center">
  <img src="assets/landscape-audit-overview.png" width="900" alt="Landscape Audit：含有中性连通分量与严格下降出口的局部景观">
</p>

Landscape Audit 从一个给定的离散候选状态出发，以确定性顺序枚举它周围的邻域。它区分“当前状态没有改进移动”和“整个中性连通分量没有改进出口”，使用独立全量目标函数核对增量变化，并输出可重放的 JSON 证书与 GraphML 图。

它是一个**审计工具**，不是另一套元启发式求解器，也不是全局景观统计指标包。

```text
x（目标值 1）--中性移动--> y（目标值 1）--严格改进--> z（目标值 0）
```

在 `x` 处，每个直接邻居都不比它更优，所以 `x` 是点局部最优。但它所在的中性平台还包含 `y`，而 `y` 存在通往 `z` 的严格下降出口，因此整个平台并非局部最优。运行 `lsaudit neutral-trap` 即可发现并认证这一区别。

## v0.1.0 功能

- 使用适配器定义的一阶或二阶移动执行确定性严格下降闭包。
- 从给定状态出发，精确执行中性平台 BFS。
- 分别给出 `point_local_optimum` 与 `plateau_local_optimum` 结论。
- 诚实报告预算截断：达到状态上限时设置 `exact: false`，绝不宣称平台闭包。
- 原子 checkpoint，并可通过 `--resume` 继续运行。
- 确定性并行移动评估；线程数不会改变证书字节。
- 逐移动检查增量目标与独立全量目标的一致性。
- 生成带 SHA-256 枚举摘要的可重放 JSON 证书。
- 导出 GraphML，供 NetworkX、Gephi 或后续景观工具使用。
- 提供三个互不相关的适配器：Max-SAT、图染色和单机排程。
- 提供专门检测“点最优/平台最优混淆”的最小 `neutral-trap` 示例。

性能热路径是一个无第三方依赖的 C++20 模板库。适配器负责状态表示、规范移动生成、移动应用、增量变化以及独立全量评估。

## 构建与运行

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure

./build/lsaudit neutral-trap \
  --threads 4 \
  --certificate out/neutral-trap.json \
  --graph out/neutral-trap.graphml
```

预期摘要：

```text
objective=1 point_local_optimum=true plateau_local_optimum=false neutral_states=2 exact=true
```

通过重新执行完整枚举来重放证书：

```bash
./build/lsaudit neutral-trap --verify out/neutral-trap.json
# VERIFIED: out/neutral-trap.json
```

运行不同领域的示例：

```bash
./build/lsaudit maxsat --radius 2 --close-first --certificate out/maxsat.json --graph out/maxsat.graphml
./build/lsaudit coloring --radius 1 --threads 4 --certificate out/coloring.json --graph out/coloring.graphml
./build/lsaudit scheduling --radius 2 --close-first --certificate out/scheduling.json --graph out/scheduling.graphml
```

## 有界审计与继续运行

中性连通分量可能呈指数级增长。状态上限统计已经完整扩展的平台状态；检查点会保存已发现状态和尚未扩展的 frontier。

```bash
./build/lsaudit neutral-trap \
  --max-states 1 \
  --checkpoint out/trap.checkpoint \
  --certificate out/partial.json \
  --graph out/partial.graphml
# 以状态码 3 退出；partial.json 中 exact=false，termination_reason=state_limit

./build/lsaudit neutral-trap \
  --max-states 100 \
  --checkpoint out/trap.checkpoint \
  --resume \
  --certificate out/complete.json \
  --graph out/complete.graphml
```

## 适配器接口

适配器是一个使用静态分派的小型类型：

```cpp
struct Adapter {
    using State = /* 精确状态类型 */;
    using Move = /* 规范移动类型 */;

    std::string instance_id() const;
    std::string neighborhood_id(std::size_t radius) const;
    std::string generator_version() const;
    std::string serialize(const State&) const;
    State deserialize(std::string_view) const;

    std::int64_t full_evaluate(const State&) const;
    std::vector<Move> moves(const State&, std::size_t radius) const;
    std::string move_key(const Move&) const;
    State apply(State, const Move&) const;
    std::int64_t delta(const State&, const Move&) const;
};
```

二阶移动由适配器直接生成。核心库不会把两个一阶移动简单组合为笛卡尔积，从而避免重复变量、顺序等价移动、执行第一次移动后失效的第二次移动，以及立即回到原状态等问题。

`full_evaluate` 应独立于增量缓存实现。在默认开启验证的情况下，每个增量预测值都必须与 `full_evaluate(apply(state, move))` 的结果一致，之后才能进入证书。

参见 [examples/adapters.hpp](examples/adapters.hpp) 和[设计说明](docs/DESIGN.md)。

## 证书语义

JSON 产物是**可重放计算证书**，而不是形式化证明证书。其核心字段包括：

- `exact`：中性连通分量是否已经穷尽；
- `termination_reason`：`exhausted` 或具体限制原因；
- `enumeration_digest`：规范排序后的移动记录的 SHA-256；
- `reference_verifier_result`：所有增量变化是否与全量重算一致；
- `point_local_optimum` 与 `plateau_local_optimum`：两个刻意分离的结论。

对于预算截断的运行，`plateau_local_optimum` 始终为 false。唯一正确的解读是“在已经扩展的部分中没有发现改进出口”，绝不能表述为“平台已经闭包”。详见[证书说明](docs/CERTIFICATES.md)。

## 项目范围与来源

Landscape Audit 来源于 Erdős Problem #617 可复现搜索项目中的工程经验。在该项目中，对单边、双边邻域和中性平台的穷举审计揭示了“一个状态闭包”与“整个中性连通分量闭包”的区别。本通用仓库不宣称解决了该数学问题，当前也尚未包含针对 `K26` 的高性能适配器。

v0.1 明确不包含完整 GUI、分布式 BFS、GPU 后端、任意 Pareto 序、任意 k 阶移动自动生成、通用对称群引擎或全局 ruggedness 指标。参见[路线图](docs/ROADMAP.md)。

## 贡献与引用

欢迎提交缺陷报告、小型适配器、证书重放测试和有界枚举改进。请先阅读 [CONTRIBUTING.md](CONTRIBUTING.md)。引用元数据位于 [CITATION.cff](CITATION.cff)。

## 许可证

MIT，详见 [LICENSE](LICENSE)。
