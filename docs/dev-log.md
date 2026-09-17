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
- C:
- D:

### 问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

-
