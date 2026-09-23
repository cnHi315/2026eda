# 开发日志

> 用法:每周每人写几行(完成 / 问题 / 下周计划)。

## 第 0 周(2026-09-10 起)

- 全组:阶段 0 —— 各自配好编译环境(CMake + wxWidgets 3.2.x),跑通最小示例工程。
- B:开始阶段一(窗口布局、TreeCtrl/PropertyGrid,可用 wxFormBuilder 拖骨架)。
- A/C/D:读 docs/data-model.md 与 docs/interfaces.md,认领各自空壳类。

## 第 1 周(2026-09-17 起)

### 本周计划

- 全组:各自跑通环境(CMake + wxWidgets),看看能否编译根目录 cmake 并运行 CircuitEditor.exe 弹出窗口。
- A:实现 `SchematicModel::addWire` + net 生成规则(端点已在某网络则并入,否则新建;拒绝重复连线与输出直连)。
- B:主窗口三栏布局 —— 菜单栏 + 左侧元件库(wxTreeCtrl)+ 中间画布 + 右侧属性面板。先空着,能显示出来即可。
- C:`ComponentLibrary` 的 types / displayName / pinTemplate;录入 AND、OR、NOT、SWITCH、LED 五个元件的引脚模板。
- D:`Simulator` 组合逻辑传播;脱离 UI,手写一个假 Schematic 对象自测"与门真值表"。

> 本周关键路径:C 的 `pinTemplate()` —— A 的 addElement 和 B 的画布都需要。C 优先交这三个函数,元件外观绘制往后放。

### 完成

> 周末回填,没做完就写做到哪了。

- A:`SchematicModel::addWire` + net 生成规则完成 —— 新建 / 并入 / 合并 / 已连通四种情况,含下标越界、自环、输出直连、重复连线四道校验,16 项自测全过。[data-model.md](data-model.md) 的 net 规则同步补全(原先漏了"合并"和"已连通")。`addElement` 暂缓,等 C 的 `pinTemplate()`。
- B:阶段一(静态骨架)完成 —— `MainFrame` 从 `main.cpp` 拆到 `src/ui/main_frame.*`;新增 `ComponentPalette`(wxTreeCtrl)/`CanvasPanel`(画布占位)/`PropertyPanel`(wxPropertyGrid),用 `wxBoxSizer(HORIZONTAL)` 挂成三栏;`CMakeLists.txt` 的 wxWidgets 组件加 `propgrid`。`cmake --build build --clean-first` 全量重建 0 warning/0 error,窗口核对通过(T-01)。
- C:任务3(ComponentLibrary 引脚模板)完成 —— 实现 `types()`、`displayName()`、`pinTemplate()`，覆盖 AND/OR/NOT/SWITCH/LED 五元件。包围盒与引脚 relPos 采用 B 画布临时坐标（AND: A(-50,-10) B(-50,10) Y(50,0)，其余按两端对称设定）。`cmake --build build -j` 全量重建 0 warning/0 error；自测 5 组类型各返回正确引脚数与方向，未知类型返回空。`docs/interfaces.md` 已补对齐表，`docs/test-plan.md` 已加核对项。已 push，供 A 的 `addElement` 接入。
- D:完成 Simulator 核心实现，含引脚状态表、网络广播与 AND 门逻辑；手写假 Schematic 跑通与门真值表 4 组用例全 PASS；新增 sim_test 独立控制台目标，并修复了静态成员误写为命名空间函数导致的 LNK2019 链接错误。终端通过：
测试: A=0, B=0 => LED=0 (期望=0) -> [PASS]
测试: A=0, B=1 => LED=0 (期望=0) -> [PASS]
测试: A=1, B=0 => LED=0 (期望=0) -> [PASS]
测试: A=1, B=1 => LED=1 (期望=1) -> [PASS]

### 问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

-
