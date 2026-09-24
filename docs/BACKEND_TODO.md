# 后端任务拆解(A/C/D 的 TODO)

> 契约:见 `src/contract/data_model.h`(改它必须全组同意,并更新 docs/data-model.md、docs/interfaces.md)。
> 约束:model/components/io/simulation 不依赖任何 wx 头文件;测试用 docs/test-plan.md 手工核对,另有 `sim_test` / `model_test` 两个控制台自测目标(`./build/…`)。
> 分工:A=架构/文件、B=GUI、C=元件库、D=仿真(对应任务见 docs/requirement.md)。

## 角色与落点

| 角色 | 负责模块 | 对应任务 | 交付 |
| --- | --- | --- | --- |
| A | 数据模型 + 文件功能 | 任务 5 | `SchematicModel`(src/model)、`NetlistIO`(src/io) |
| B | 用户界面 + 绘图编辑 | 任务 2、4 | src/main.cpp、src/ui(路线图见 IMPLEMENTATION_ROADMAP.md) |
| C | 元件库 | 任务 3 | `ComponentLibrary`、各门元件(src/components) |
| D | 电路仿真 | 任务 1、6 | `Simulator`(src/simulation) |

---

## A — 数据模型与文件(src/model + src/io)

- [x] A1 实现 `SchematicModel` 增删改查(addElement / removeElement / moveElement)。**已完成**(1925649):`addElement` 的引脚模板取自 C 的 `pinTemplate()`。核对:test-plan T-02。
- [x] A2 实现 addWire / removeWire 及 net 生成规则(端点已在某网络则并入,否则新建;不允许输出直连/重复连线)。**已完成**:addWire 覆盖新建/并入/合并/已连通四种情况 + 四道校验;`removeWire(wireId)` 的语义按实际使用场景改为**删一条导线**(参数是 `Wire::id`),删完由 `rebuildNets()` 整体重算 nets。核对:test-plan T-03。
- [ ] A3 `NetlistIO::save / load`:JSON 往返,id 保持稳定。开工时引入 nlohmann/json 单头文件放到 src/io/。核对:test-plan T-04。
- [ ] A4 `NetlistIO::exportNetlist`:文本网表。**先导出 KiCad 的网表研究它的格式**,格式说明补进 docs/data-model.md。核对:test-plan T-05。

## C — 元件库(src/components)

- [x] C1 实现 `ComponentLibrary`:types() / displayName() / pinTemplate()。先做 AND、OR、NOT、SWITCH、LED 五个。
- [ ] C2 确定各元件外观与引脚相对坐标(relPos),供 B 画符号与命中检测用。**relPos 已定**(见 docs/interfaces.md 的引脚对齐表);外观尺寸待与 B 确认。
- [ ] C3 自定义元件(任务 3 要求):用 JSON 文件描述元件,读取后进库(选做)。

## D — 仿真(src/simulation)

- [x] D1 组合电路传播:给定输入,输出沿拓扑稳定。**AND / OR / NOT 已实现**,与门真值表 4 组跑通。核对:test-plan T-06/T-07。
- [ ] D2 事件队列:输入变化 → 受影响网络 → 依次更新输出 → 回调 UI。
- [ ] D3 环路 / 振荡检测(选做):识别组合环并报告。
- [ ] D4 时序元件(选做):D 触发器、时钟。

> 交互参考:Logisim(拨开关、导线变色)—— 这就是任务 6 要的效果。

---

## B — UI(略)

见 docs/IMPLEMENTATION_ROADMAP.md。
