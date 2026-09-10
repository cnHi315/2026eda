# Backend (Core) 任务拆解 · 契约驱动开发

> 契约接口：见 `include/core_interfaces.h`（ui ↔ core 的唯一耦合边界）。
> 约束：全程 TDD（红 → 绿 → 重构），每个切片先写失败测试再实现；core 不依赖任何 wx 类型。
> 接口实现分工：
>   - Role A 实现 `ICircuitElement` 与 `INetlistModel`。
>   - Role B 实现仿真引擎，并通过 `ISimulationObserver` 回调通知 ui。
>   - Role C 实现网表序列化 / 反序列化（文件功能）。

## 角色分配

| 角色 | 负责模块 | 对应任务 | 交付接口 / 模块 |
|---|---|---|---|
| Role A | 数据与元件库 | 任务 3 | `ICircuitElement`、`INetlistModel`、`ComponentLibrary` |
| Role B | 仿真引擎 | 任务 1、任务 6 | `SimulationEngine`（通知 `ISimulationObserver`） |
| Role C | 文件功能 / 网表导入导出 | 任务 5 | Netlist 序列化 / 反序列化 |

---

## Role A — 数据与元件库（任务 3）

- [ ] A1 `SignalValue` 值类型：位宽 + 数值 + 未定义态。TDD：各门真值表在值级别成立，位宽传播正确。
- [ ] A2 `ComponentDefinition` 与基础门评估（AND / OR / NOT / XOR / NAND / NOR）。TDD：每个门输入→输出真值全对。
- [ ] A3 `ComponentLibrary` 注册与按 `type()` 标识实例化。TDD：库能检索并实例化全部定义。
- [ ] A4 实现 `ICircuitElement`（坐标 + 引脚）与 `INetlistModel` 命令（addElement / addWire / moveElement / removeElement）。TDD：命令后网表结构一致，拓扑校验（两输出并接 / 悬空输入 / 环路）报错。

## Role B — 仿真引擎（任务 1、任务 6）

- [ ] B1 事件队列 `EventQueue`：入队 / 出队 / 优先级顺序。TDD：按时间与优先级正确弹出。
- [ ] B2 组合电路传播：给定输入，输出沿拓扑正确稳定。TDD：多级门级联结果正确。
- [ ] B3 时序元件 / 时钟 / 输入源（D 触发器、时钟、常量源）。TDD：时钟驱动下时序行为正确。
- [ ] B4 环路 / 振荡检测 + 通过 `ISimulationObserver::onSignalChanged` 回调电平变化。TDD：震荡被识别，且值变化触发回调。

## Role C — 文件功能 / 网表导入导出（任务 5）

- [ ] C1 定义版本化网表序列化格式（schema 与版本号）。TDD：格式生成 / 解析可往返。
- [ ] C2 导出（Save）：Netlist → 文件。TDD：导出后重新导入，结构与值等价。
- [ ] C3 导入（Load）：文件 → Netlist，重建元件与连线。TDD：往返等价（round-trip）。
- [ ] C4 错误处理与校验：格式非法 / 版本不兼容 / 未知元件类型。TDD：坏输入被拒绝并报错，不崩溃。