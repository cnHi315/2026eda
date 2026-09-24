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
- B:阶段三(交互与布线核心)完成 —— `CanvasPanel` 加交互状态机(`Idle / DraggingComponent / DrawingWire`)与命中检测(优先级 引脚 → 元件 → 导线;引脚半径 8、导线容差 4、吸附步长 20,均为逻辑坐标);元件拖拽实时跟随、抬起提交、单击不产生位移、Esc 还原;引脚到引脚橡皮筋连线,校验规则与 A 的 `addWire` 对齐(自环 / 输出直连 / 重复连线正反都算);选中态用蓝色外框、引脚悬停与吸附用橙色实心点、选中导线加粗变蓝;新增 `SetStatusCallback`,把"选中 / 拖拽中 / 连线中 / 失败原因"回写状态栏(补掉阶段一遗留的"状态栏随操作更新")。三个写入口 `MoveComponentTo / CanConnect / AddWire` 已标注阶段四替换点(`model.moveElement / model.addWire`)。验证:`cmake --build build --clean-first --target CircuitEditor` 0 warning;test-plan T-09 机器化核对 12/12 通过。未做:接入 `SchematicModel`(阶段四)、按类型画真符号。
- C:
- D:

### 第 2 周问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

**B 与其他负责人后续对接的潜在问题**

- **与 A(数据模型 / 文件)**
  - `addElement / moveElement / removeElement / removeWire` 与 `NetlistIO(save/load/exportNetlist)` 尚未实现,阶段四"从元件库放置元件、拖拽回写、删除、保存/打开/导出网表"无法联调;目前菜单只有 `File → Exit`。
  - `addWire` 失败语义要统一:阶段三 `CanConnect()` 已按 data-model.md 实现四道校验(自环 / 输出直连 / 重复连线正反都算),阶段四换成 `model.addWire` 后必须同一套规则;现在失败只返回空串,UI 拿不到原因,建议返回失败原因,或约定"UI 先自查、只把成功路径交给 model"。
  - `SchematicModel` 没有变更通知/脏标记,B 只能每次操作整幅重绘;若 A 后续加缓存或信号机制,请先约定接口。
  - id 稳定性:选中态、导线端点、拖拽都按 `componentId + pinIndex` 索引,`load()` 后 id 必须保持不变(JSON 往返),否则选中态与导线会失效。
  - 契约 `data_model.h` 按 README 新约定"要改先问人":阶段四若需要新字段(如导线的 netId、元件显示名覆盖),要先全组同步。
- **与 C(元件库)**
  - **包围盒与命中参数没定死**(本周关键路径):C 给 `relPos(±50)`,B 给命中半径 8 / 导线容差 4 / 吸附步长 20。建议把"每类型包围盒宽高 + 引脚可视长度 + 命中半径"一起写进 `docs/interfaces.md`,否则阶段四画真符号时 UI 和 A 的 `addElement` 都要返工。
  - `types()/displayName()` 还没接进 UI:左侧元件树现在写死英文类型串 `AND/OR/NOT/SWITCH/LED`,阶段四要换成 `types()` + 中文 `displayName()`;树里显示中文、画布标签显示 id,这个口径要统一(用户手册同步)。
  - 自定义元件(任务 3 的 C3)若走 JSON 描述,UI 需要知道"外观怎么画"和"未知类型怎么兜底",否则画布只能一直画矩形。
- **与 D(仿真)**
  - **手势冲突要先约定**:Logisim 式"拨开关"是点一下翻转电平,而阶段三已把左键按下用于拖动/连线。建议定成"左键拖动/连线,拨开关用双击或右键",否则阶段四两套交互会抢事件。
  - `Simulator` 目前只实现 AND(OR/NOT 未做),SWITCH/LED 也没有专门逻辑;`setSignalCallback` 的触发时机(每步 / 仅变化)、`query()` 在 `step()` 之前的语义都未定,而 B 要用它刷新导线颜色与 LED 亮灭。
  - 导线颜色需要"引脚 → 网络 → 电平"的映射:B 侧 `Wire` 只有两端 `PinRef`,net 由 A 生成;要么 UI 读 A 的 `nets`,要么 D 的回调按引脚聚合后让 UI 反查导线。
  - 回调频率未定:B 计划用 `RefreshRect()` 局部刷新而不是整幅重绘,需要 D 说明一次 `step()` 最多触发多少次回调。
- **全组 / 流程**
  - `CMakeLists.txt` 与 `data_model.h` 按新约定"要改先问人":阶段四若要把验证台/测试目标收进仓库,需要全组同意(B 的验证台现在放在仓库外)。
  - 阶段三的命中检测依赖"包围盒 + 引脚长度 + 吸附步长"这组参数,阶段四任何一处调整都要同时改 UI 与元件库,建议第 3 周内定死。
