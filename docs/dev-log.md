# 开发日志

> 用法:每周每人写几行(完成 / 问题 / 下周计划)。

## 第 0 周(2026-09-10 起)

- 全组:阶段 0 —— 各自配好编译环境(CMake + wxWidgets 3.2.x),跑通最小示例工程。
- B:开始阶段一(窗口布局、TreeCtrl/PropertyGrid,可用 wxFormBuilder 拖骨架)。
- A/C/D:读 docs/data-model.md 与 docs/interfaces.md,认领各自空壳类。

## 第 1 周(2026-09-17 起)

### 第 1 周计划

- 全组:各自跑通环境(CMake + wxWidgets),看看能否编译根目录 cmake 并运行 CircuitEditor.exe 弹出窗口。
- A:实现 `SchematicModel::addWire` + net 生成规则(端点已在某网络则并入,否则新建;拒绝重复连线与输出直连)。
- B:主窗口三栏布局 —— 菜单栏 + 左侧元件库(wxTreeCtrl)+ 中间画布 + 右侧属性面板。先空着,能显示出来即可。
- C:`ComponentLibrary` 的 types / displayName / pinTemplate;录入 AND、OR、NOT、SWITCH、LED 五个元件的引脚模板。
- D:`Simulator` 组合逻辑传播;脱离 UI,手写一个假 Schematic 对象自测"与门真值表"。

> 本周关键路径:C 的 `pinTemplate()` —— A 的 addElement 和 B 的画布都需要。C 优先交这三个函数,元件外观绘制往后放。

### 第 1 周完成

> 周末回填,没做完就写做到哪了。

- A:`SchematicModel::addWire` + net 生成规则完成 —— 新建 / 并入 / 合并 / 已连通四种情况,含下标越界、自环、输出直连、重复连线四道校验,16 项自测全过。[data-model.md](data-model.md) 的 net 规则同步补全(原先漏了"合并"和"已连通")。`addElement` 暂缓,等 C 的 `pinTemplate()`。
- B:阶段一(静态骨架)完成 —— `MainFrame` 从 `main.cpp` 拆到 `src/ui/main_frame.*`;新增 `ComponentPalette`(wxTreeCtrl)/`CanvasPanel`(画布占位)/`PropertyPanel`(wxPropertyGrid),用 `wxBoxSizer(HORIZONTAL)` 挂成三栏;`CMakeLists.txt` 的 wxWidgets 组件加 `propgrid`。`cmake --build build --clean-first` 全量重建 0 warning/0 error,窗口核对通过(T-01)。
- C:任务3(ComponentLibrary 引脚模板)完成 —— 实现 `types()`、`displayName()`、`pinTemplate()`，覆盖 AND/OR/NOT/SWITCH/LED 五元件。包围盒与引脚 relPos 采用 B 画布临时坐标（AND: A(-50,-10) B(-50,10) Y(50,0)，其余按两端对称设定）。`cmake --build build -j` 全量重建 0 warning/0 error；自测 5 组类型各返回正确引脚数与方向，未知类型返回空。`docs/interfaces.md` 已补对齐表，`docs/test-plan.md` 已加核对项。已 push，供 A 的 `addElement` 接入。
- D:完成 Simulator 核心实现，含引脚状态表、网络广播与 AND 门逻辑；手写假 Schematic 跑通与门真值表 4 组用例全 PASS；新增 sim_test 独立控制台目标，并修复了静态成员误写为命名空间函数导致的 LNK2019 链接错误。终端通过：
测试: A=0, B=0 => LED=0 (期望=0) -> [PASS]
测试: A=0, B=1 => LED=0 (期望=0) -> [PASS]
测试: A=1, B=0 => LED=0 (期望=0) -> [PASS]
测试: A=1, B=1 => LED=1 (期望=1) -> [PASS]

### 第 1 周问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

-

## 第 2 周(2026-09-24 起)

### 第 2 周计划

- **A**:`SchematicModel` 增删改查 —— `removeWire` → `moveElement` → `removeElement`(最后这个最绕,要连带断开元件上的线);`addElement` 接上 C 的 `pinTemplate()`。
- **B**:画布 —— `wxScrolledWindow` + `wxPaintDC` 画网格;鼠标左键点击打印坐标;按 C 的引脚坐标画出与门等元件的符号形状。
- **C**:和 B 一起把元件符号的尺寸定死(±50 这个基准一旦改,B 的画布和命中检测都要跟着动);顺带核对五元件的引脚表。
- **D**:与 A 的模块对接一次 —— 用 `addWire` 造出的真实数据跑仿真(全项目第一次两个模块合体);再接 `setSignalCallback` 回调。

> 上周的关键路径(C 的 `pinTemplate()`)已经通了。
> 本周的关键路径变成 **B 和 C 的符号尺寸** —— 定不下来,A 的 `addElement` 和 B 自己的画布都要反复改。

### 第 2 周完成

> 周末回填,没做完就写做到哪了。

- A:
- B:阶段二(只读画布渲染)完成 —— `CanvasPanel` 从占位面板改成 `wxPanel` 自绘:`wxAutoBufferedPaintDC` 双缓冲 + `OnPaint` 里"网格 → 导线 → 元件"分层绘制;网格 20 逻辑像素、每 5 格一条粗线;按 C 的 `pinTemplate` relPos(±50)画 4 个假元件(SW1/SW2/U1(AND)/LED1:矩形 + 引脚短线 + 引脚名)与 3 条假导线;逻辑坐标↔屏幕像素换算集中在 `ToScreen()`/`ToLogical()`,阶段三命中检测直接复用。`cmake --build build --clean-first --target CircuitEditor` 干净重建 0 warning/0 error;实跑截图核对网格、元件符号、导线与三栏布局,阶段二 4 项通过(见 test-plan T-08)。未做:元件库点击放置、鼠标交互、真实数据接入(阶段三/四)。
- C:
- D:后端模拟核心与测试闭环完成 —— `Simulator` 实现组合逻辑两步迭代传播，支持 **AND、OR、NOT** 基础门电路；完善 `load()`、`setInput()`、`query()` 及带 **OnChange 防抖优化**（电平变化才触发）的 `setSignalCallback` 机制；完成 `sim_test` 编译及真值表全功能单元测试。未做：与 A 模块网表动态数据的全链路合体联调（接口已备好，交由 A 侧推进）。

### 第 2 周问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

- 全量构建(`cmake --build build -j` 即 `./build.sh`)在 Linux 上被测试目标打断:`tests/main_test.cpp:5` 直接 `#include <windows.h>` 并调 `SetConsoleOutputCP`,而 D 的 `sim_test` 目标进了默认 `all`,报 `fatal error: windows.h: No such file or directory`。绕行:加 `--target CircuitEditor`。建议 D 加 `#ifdef _WIN32` 守卫,或给 `sim_test` 加 `EXCLUDE_FROM_ALL`(细节见群里的 review 意见)。
