# 架构设计说明

状态:□草稿 ☑评审中 □稳定 | 负责人:A | 最后更新:2026-09-17(每次修改后更新这行)

## 简述

一个可执行程序,代码按模块分目录:UI 通过唯一契约 `src/contract/data_model.h` 与四个功能模块对接。

## 模块与依赖

```bash
ui ─┬─▶ model ──────┐
    ├─▶ io          ├─▶ components
    └─▶ simulation ─┘

所有模块 ─▶ contract/data_model.h(唯一契约)
```

依赖规则:

- **ui 调用其余四个模块**;**components 是叶子模块**,model 和 simulation 依赖它(分别取引脚模板、取逻辑功能);io 只依赖 contract。
- **除 ui 外不许 include wx 头文件** —— 谁都能单独编译自己那块。
- **不做独立的 Controller 层**:SchematicModel 的方法就是命令入口,ui 直接调用(做大了再考虑分层)。
- 数据流:ui 改数据 → model;保存/导出 → io;仿真 → simulation 读 Schematic,回调通知 ui 刷新。

## 模块职责

| 模块 | 职责 | 不负责 |
| --- | --- | --- |
| src/ui | 窗口、菜单、工具栏、画布、鼠标交互 | 数据结构、仿真算法、文件格式 |
| src/model | 元件/导线/网络的增删改查 | 界面、文件、仿真 |
| src/components | 元件类型、引脚模板、外观绘制信息 | 画布交互 |
| src/io | JSON 保存/打开、网表导出 | 界面 |
| src/simulation | 逻辑传播、电平查询、回调通知 | 修改原理图数据 |

## 关键流程

- **放置元件**:面板选类型 → 画布点击 → `SchematicModel::addElement` → 刷新
- **连线**:点引脚 A → 拖到引脚 B → `SchematicModel::addWire`(自动生成/合并 net)→ 刷新
- **保存/打开**:`NetlistIO::save/load(Schematic, 路径)` → JSON 文件
- **仿真**:`Simulator::load(原理图)` → `setInput`(拨开关)→ `step()` → 回调通知 ui 更新 LED

## 技术选型

| 项 | 选择 | 说明 |
| --- | --- | --- |
| 语言 | C++17 | 项目标准 |
| GUI | wxWidgets 3.2.2 | 界面骨架可用 wxFormBuilder 拖出 |
| 构建 | CMake(唯一 CMakeLists.txt) | 加新 .cpp 自动收集 |
| 文件 | JSON(开工时加 nlohmann/json 单头文件) | 由 A 引入 |
| 网表 | 简化文本网表 | 格式参考 KiCad,由 A 研究 |
| 仿真 | 事件队列 + 组合逻辑传播 | 交互参考 Logisim |
