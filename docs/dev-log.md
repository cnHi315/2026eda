# 开发日志

> 用法:每周每人写几行(完成 / 问题 / 下周计划)。

## 第 0 周(2026-09-10 起)

- **全组**:阶段 0 —— 各自配好编译环境(CMake + wxWidgets 3.2.x),跑通最小示例工程。
- **B**:开始阶段一(窗口布局、TreeCtrl/PropertyGrid,可用 wxFormBuilder 拖骨架)。
- **A/C/D**:读 docs/data-model.md 与 docs/interfaces.md,认领各自空壳类。

## 第 1 周(2026-09-17 起)

### 第 1 周计划

- **全组**:各自跑通环境(CMake + wxWidgets),看看能否编译根目录 cmake 并运行 CircuitEditor.exe 弹出窗口。
- **A**:实现 `SchematicModel::addWire` + net 生成规则(端点已在某网络则并入,否则新建;拒绝重复连线与输出直连)。
- **B**:主窗口三栏布局 —— 菜单栏 + 左侧元件库(wxTreeCtrl)+ 中间画布 + 右侧属性面板。先空着,能显示出来即可。
- **C**:`ComponentLibrary` 的 types / displayName / pinTemplate;录入 AND、OR、NOT、SWITCH、LED 五个元件的引脚模板。
- **D**:`Simulator` 组合逻辑传播;脱离 UI,手写一个假 Schematic 对象自测"与门真值表"。

> 本周关键路径:C 的 `pinTemplate()` —— A 的 addElement 和 B 的画布都需要。C 优先交这三个函数,元件外观绘制往后放。

### 第 1 周完成

> 周末回填,没做完就写做到哪了。

- **A**:`SchematicModel::addWire` + net 生成规则完成 —— 新建 / 并入 / 合并 / 已连通四种情况,含下标越界、自环、输出直连、重复连线四道校验,16 项自测全过。[data-model.md](data-model.md) 的 net 规则同步补全(原先漏了"合并"和"已连通")。`addElement` 暂缓,等 C 的 `pinTemplate()`。
- **B**:阶段一(静态骨架)完成 —— `MainFrame` 从 `main.cpp` 拆到 `src/ui/main_frame.*`;新增 `ComponentPalette`(wxTreeCtrl)/`CanvasPanel`(画布占位)/`PropertyPanel`(wxPropertyGrid),用 `wxBoxSizer(HORIZONTAL)` 挂成三栏;`CMakeLists.txt` 的 wxWidgets 组件加 `propgrid`。`cmake --build build --clean-first` 全量重建 0 warning/0 error,窗口核对通过(T-01)。
- **C**:任务 3(ComponentLibrary 引脚模板)完成 —— 实现 `types()`、`displayName()`、`pinTemplate()`,覆盖 AND/OR/NOT/SWITCH/LED 五元件。包围盒与引脚 relPos 采用 B 画布临时坐标(AND: A(-50,-10) B(-50,10) Y(50,0),其余按两端对称设定)。`cmake --build build -j` 全量重建 0 warning/0 error;自测 5 组类型各返回正确引脚数与方向,未知类型返回空。`docs/interfaces.md` 已补对齐表,`docs/test-plan.md` 已加核对项。已 push,供 A 的 `addElement` 接入。
- **D**:完成 Simulator 核心实现,含引脚状态表、网络广播与 AND 门逻辑;手写假 Schematic 跑通与门真值表 4 组用例全 PASS;新增 `sim_test` 独立控制台目标,并修复了静态成员误写为命名空间函数导致的 LNK2019 链接错误。终端输出:

  ```text
  测试: A=0, B=0 => LED=0 (期望=0) -> [PASS]
  测试: A=0, B=1 => LED=0 (期望=0) -> [PASS]
  测试: A=1, B=0 => LED=0 (期望=0) -> [PASS]
  测试: A=1, B=1 => LED=1 (期望=1) -> [PASS]
  ```

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

- **A**:`SchematicModel` 增删改查完成 —— `addElement`(引脚模板取自 C 的 `pinTemplate()`,id 前缀 U/SW/LED)、`removeWire`、`moveElement`、`removeElement`。`removeWire` 的语义按实际使用场景从「删整个网络」改成「删一条导线」:接错一根线不该整个重来。新增私有 `rebuildNets()` —— 每条线当一条边求连通分量,删线/删元件后整体重算 `nets`,保证"两引脚相连 ⟺ 在同一网络";`moveElement` 只改坐标、不进重算(`Wire` 存的是 `PinRef` 不是坐标)。新增 `tests/model_test.cpp` + `model_test` 目标(照 `sim_test` 写法),57 项自测全过,`sim_test` 不受影响。`docs/data-model.md`、`docs/interfaces.md` 同步。未做:`NetlistIO`。
- **B**:阶段二(只读画布渲染)完成 —— `CanvasPanel` 从占位面板改成 `wxPanel` 自绘:`wxAutoBufferedPaintDC` 双缓冲 + `OnPaint` 里"网格 → 导线 → 元件"分层绘制;网格 20 逻辑像素、每 5 格一条粗线;按 C 的 `pinTemplate` relPos(±50)画 4 个假元件(SW1/SW2/U1(AND)/LED1:矩形 + 引脚短线 + 引脚名)与 3 条假导线;逻辑坐标↔屏幕像素换算集中在 `ToScreen()`/`ToLogical()`,阶段三命中检测直接复用。`cmake --build build --clean-first --target CircuitEditor` 干净重建 0 warning/0 error;实跑截图核对网格、元件符号、导线与三栏布局,阶段二 4 项通过(见 test-plan T-08)。未做:元件库点击放置、鼠标交互、真实数据接入(阶段三/四)。
- **B**:阶段三(交互与布线核心)完成 —— `CanvasPanel` 加交互状态机(`Idle / DraggingComponent / DrawingWire`)与命中检测(优先级 引脚 → 元件 → 导线;引脚半径 8、导线容差 4、吸附步长 20,均为逻辑坐标);元件拖拽实时跟随、抬起提交、单击不产生位移、Esc 还原;引脚到引脚橡皮筋连线,校验规则与 A 的 `addWire` 对齐(自环 / 输出直连 / 重复连线正反都算);选中态用蓝色外框、引脚悬停与吸附用橙色实心点、选中导线加粗变蓝;新增 `SetStatusCallback`,把"选中 / 拖拽中 / 连线中 / 失败原因"回写状态栏(补掉阶段一遗留的"状态栏随操作更新")。三个写入口 `MoveComponentTo / CanConnect / AddWire` 已标注阶段四替换点(`model.moveElement / model.addWire`)。验证:`cmake --build build --clean-first --target CircuitEditor` 0 warning;test-plan T-09 机器化核对 12/12 通过。未做:接入 `SchematicModel`(阶段四)、按类型画真符号。
- **C**:
- **D**:后端模拟核心与测试闭环完成 —— `Simulator` 实现组合逻辑两步迭代传播,支持 **AND、OR、NOT** 基础门电路;完善 `load()`、`setInput()`、`query()` 及带 **OnChange 防抖优化**(电平变化才触发)的 `setSignalCallback` 机制;完成 `sim_test` 编译及真值表全功能单元测试。未做:与 A 模块网表动态数据的全链路合体联调(接口已备好,交由 A 侧推进)。

### 第 2 周问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

#### B 与其他负责人后续对接的潜在问题

- **与 A(数据模型 / 文件)**
  - `SchematicModel` 的增删改查已补齐(1925649,57 项自测全过),阶段四可以直接接;**`NetlistIO` 仍未开工** —— 保存 / 打开 / 导出网表暂时无法联调,目前菜单只有 `File → Exit`。
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
  - **A 补充**:`addWire` 的「输出直连」只查被连的那一对引脚,不查整个网络 —— 两个输出可以经由一个输入间接进同一网络(`SW1.Y — U1.B — SW2.Y`)。而 `Simulator::step()` 对同一网络取「第一个非 Undefined 的电平」广播,会静默地谁先谁赢。B 的 `CanConnect()` 有同一个洞。归属待定。
- **全组 / 流程**
  - `CMakeLists.txt` 与 `data_model.h` 按新约定"要改先问人":阶段四若要把验证台/测试目标收进仓库,需要全组同意(B 的验证台现在放在仓库外)。
  - 阶段三的命中检测依赖"包围盒 + 引脚长度 + 吸附步长"这组参数,阶段四任何一处调整都要同时改 UI 与元件库,建议第 3 周内定死。

### 第 2 周已解决

- 全量构建在 Linux 上被测试目标打断(`tests/main_test.cpp` 直接 `#include <windows.h>`,`sim_test` 又进了默认 `all`)。9e6e58d / 070434d 已修:加 `#ifdef _WIN32` 守卫,测试函数移出 `src/`。教训见 README 第六节。

## 第 3 周(2026-10-01 起)

> 国庆假期,全组无进度(9/24 之后没有新提交)。

## 第 4 周(2026-10-08 起)

### 第 4 周计划

- **A**:`NetlistIO` —— `save` / `load`(JSON 往返,id 保持不变)/ `exportNetlist`。引入 nlohmann/json 单头文件到 `src/io/`;导出格式先照着 KiCad 的网表研究。加一个 `io_test` 目标(照 `sim_test` / `model_test` 的写法)。另外牵头把 SWITCH/LED 的引脚模型定下来(见下面的硬阻塞)。
- **B**:**阶段四(全项目第一次真数据上屏)** —— 删掉 `m_demo` 假数据,`OnPaint` 改读 `model.data()`;`MoveComponentTo / CanConnect / AddWire` 换成 `model.moveElement / model.addWire`;新增"元件树点击放置 → `model.addElement`"与 Delete → `model.removeElement`;接 `setSignalCallback` 刷新导线颜色与 LED;文件菜单接 A 已交付的 `NetlistIO`(`save` / `load` / `loadFrom` / `exportNetlist`)。**几何参数不再等 C**:包围盒 60×40、引脚可视长度 20、引脚命中半径 8、导线命中容差 4、吸附步长 20 由 B 定为 v1(与 C 的 relPos ±50 自洽,50 = 30 + 20),评审通过后补进 `docs/interfaces.md`,C 只需追认、**不需要改 C 的代码**;元件外观 v1 一律"矩形 + 引脚名",真符号(弧 / 圆圈 / 拨杆)留 v1.1。SWITCH/LED 引脚模型未定**不阻塞**:UI 拨开关时对该元件的所有引脚置同一电平。
- **C**:
  - ① **和 B 把符号尺寸定死** —— 上周的关键路径,没做完。包围盒宽高 / 引脚可视长度 / 命中半径 / 吸附步长,一并写进 `docs/interfaces.md` 的引脚对齐表旁边。定不下来,B 的画布和 A 的 `addElement` 都要返工。
  - ② **和 A、D 统一 SWITCH/LED 的引脚模型**(见下面的钉子)。
  - ③ **给五个元件补外观描述**:每种类型怎么画 —— 与门 / 或门的外轮廓(矩形还是弧)、非门输出端的圆圈、开关的拨杆、LED 的三角加两根发光箭头。B 现在只能一律画矩形;要么 C 给出几何参数(折线 / 圆 / 弧),要么全组约定"v1 一律矩形 + 引脚名,真符号往后放"。
  - ④ **定"未知类型怎么办"**:`pinTemplate()` 现在对不认识的类型返回空,画布得知道这是什么意思(建议:返回空 = 拒绝放置,状态栏给提示),否则接 C3 时会踩。
  - ⑤ **和 B 统一命名口径**:元件树里显示中文 `displayName()`,画布上显示 `id`;顺手同步 `docs/user-manual.md`。
  - ⑥ **C3 自定义元件**:用 JSON 描述元件、读取后进库。
- **D**:`Simulator` 接真实数据 —— 用 `SchematicModel::addWire` 造一份电路喂给 `load()`,跑通 `setInput → step → query` 全链路(现在 `sim_test` 用的是手写假数据);定死 `setSignalCallback` 的触发时机与单次 `step()` 的回调上限(B 要拿它做局部刷新)。

> 本周关键路径:**B 的阶段四** —— 用户看得见的东西全要经过它。它的前置(C 的符号尺寸)已改为由 B 自行定稿、C 追认即可,不再卡人;SWITCH 引脚模型用 UI 兜底。
> **先拔这颗钉子:SWITCH 的引脚模型对不上。** C 的 `pinTemplate("SWITCH")` 给两个引脚(`[0]="A"` 输入、`[1]="Y"` 输出);D 的 `sim_test` 却按"单引脚、pinIndex 0 就是输出"写。`model.addElement("SWITCH")` 造出来的开关有 2 个脚,`setInput("SW1", 0, …)` 会设到输入脚 `A` 上 —— 而 `A` 不在任何网络里,电平传不出去,**拨开关这个动作是死的**。
> 建议:SWITCH 收敛成 1 个引脚(输出)、LED 收敛成 1 个引脚(输入)—— 开关是源、LED 是汇,物理上也说得通。要改 C 的 `pinTemplate`、D 的测试,和 `docs/interfaces.md` 的引脚对齐表,三方点头。

### 第 4 周完成

> 周末回填,没做完就写做到哪了。

- **A**:`NetlistIO` 完成 —— `save` / `load`(nlohmann/json 3.11.3 单头放进 `src/io/json.hpp`;JSON 往返后 id / 引脚 / 坐标全不变,坏文件不破坏原数据)、`exportNetlist`(输出 KiCad 的 s-expression 网表,Pcbnew 可导入;引脚按**名字**索引,`tstamp` 由元件 id 哈希而来,所以导出结果稳定、可 diff)。新增 `SchematicModel::loadFrom()` —— 打开文件时把读进来的原理图换进去,`nets` 一律按 `wires` 重算:文件里存的 nets 不作数,否则手改过的文件会让网络和导线对不上。新增 `tests/io_test.cpp` + `io_test` 目标,**61 项自测全过**;另写了个 Python 校验器当外援,把导出的 `.net` 当真正的 s-expression 解析、再和同一份 `.json` 交叉对照,**50/50 通过**。踩到两个只在 Windows 上出现的跨平台坑(已写进 README 第六节第 7 条):MinGW 的 `ifstream` 打开失败**不置 failbit**,`if (!in)` 失效;Windows 的 `std::rename` 目标已存在时会失败。网表格式说明写在 `src/io/netlist_io.h` 的注释里,不占 docs;`docs/interfaces.md` 只加了 `loadFrom` 一行。另外定了一条:**「保存时顺手导出网表」是 UI 层的事**(B 的保存菜单项里一次点击调 `save` + `exportNetlist` 两个函数),`io` 层保持两个独立函数 —— 需求表 F-03 / F-04 是两条独立验收项,代码里分开,答辩好指。未做:UI 侧的菜单项(属 B 的阶段四)。
- **B**:阶段四(真数据上屏)完成 —— `CanvasPanel` 改读 `SchematicModel::data()`,删掉 `m_demo` / `BuildDemoSchematic`;写操作全部走 model(`addElement / moveElement / addWire / removeWire / removeElement`),失败与非法操作在状态栏给原因;元件树改由 `ComponentLibrary::types() / displayName()` 填充(中文),选中类型 → 画布点击放置(吸附 20 栅格,支持连续放置,Esc 退出);Delete 删除选中元件/导线;双击 SWITCH 拨动电平(默认关闭,v1 不写进 JSON),每次变化 `Simulator::load(data) + setInput + step()`,导线按电平着色(高=亮绿 3px / 低=暗绿 2px / 未知=灰 2px)、LED 亮灭、开关底色变化;属性表显示选中元件(只读,含引脚表);文件菜单与工具栏接 `NetlistIO`(新建 / 打开 / 保存 / 另存为 / 导出网表),状态栏第二格实时显示"元件 / 导线 / 网络"计数。验证:全量构建 0 warning;model_test 57/57、io_test 61/61、sim_test PASS;test-plan T-10 端到端机器化核对 15/15。踩坑记录:非 ASCII 文本必须走 `U8()`(FromUTF8),窗口标题原写成窄字面量 `"Circuit Editor — "`,在这台机器的 locale 下被吞成空串,标题只剩文件名。
- **B(issue 1 / 4)**:画布体验与性能补齐 —— ① **滚动/平移**:中键拖动 + 滚轮 / Shift+滚轮(触摸板横扫)平移视图,`m_origin` 即平移量并做"内容至少留 40px 在视口内"的夹取;网格线锚定逻辑原点(平移非整格时整体跟着走),坐标换算仍集中在 `ToScreen()/ToLogical()`,所以命中检测与拖拽在平移状态下不用改任何代码。② **局部重绘**:拖拽只刷"旧位置 ∪ 新位置(含相连导线)"、橡皮筋只刷"上一帧 ∪ 当前帧"、悬停只刷前后两个元件;网格绘制按 DC 的 `GetClippingBox()` 限定循环范围;`Find()` 加 id→指针索引缓存(结构变化时标脏重建),避免每帧 O(n) 线性查找。验证:test-plan T-11 机器化核对 11/11(位移量精确到像素、平移后命中/拖动仍准、原位置无残影、网格随平移整体位移、滚轮一格 40px);性能 400 元件 13.5ms/帧、1000 元件 32.8ms/帧(含拖拽处理与绘制)。
- **B(交接与验证工具入库)**:把上下文交接整理成 `docs/HANDOFF-B.md`(工作区同级 `/home/shuai/EDA/HANDOFF.md` 有同内容入口),并把 GUI 端到端验证工具收进 `tools/`:`ui_verify.cpp`(起真实 `UiMainFrame`、把 `wxMouseEvent/wxKeyEvent` 注入画布事件链)、`verify_stage4.py`(阶段四 15 项)、`verify_issues14.py`(issue 1/4 共 11 项)、`tools/README.md`(编译方式、跑法、抓像素的坑)。**这些工具不进 CMake 构建**(`GLOB_RECURSE` 只收 `src/`),不影响 `CircuitEditor`。之所以不用 XTest:本机 Wayland 会话下指针 warp 与点击都投递不到 X 客户端,自动化会静默不生效。复核:入库后重跑两套脚本,15/15 与 11/11 全绿。
- **B(合并 main 时顺手修的构建阻塞)**:D 的 `src/simulation/simulator.cpp` 末尾那段"任务#1 预留的 SchematicModel 联调测试"漏了注释里说的 `#if 0`,而且 `#include "../model/schematic_model.h"` 落在 `namespace editor` **内部** —— 头文件里的命名空间被二次嵌套,编译器把类名解析成 `editor::editor::SchematicModel`,**整个仓库在 Linux 与 Windows 都编不过(main 上同样是坏的)**。B 按注释原意补上 `#if 0 / #endif` 恢复构建,并在注释里写明"真正启用时应按 README 约定把这段挪到 `tests/`"。合并后复核:全量构建 0 warning、model/io/sim 三套单测通过、阶段四端到端 **15/15**。
- **C**:第 4 周追认与文档定稿 —— ① 追认 B 的几何参数(包围盒 60×40、引脚可视长度 20、命中半径 8、导线容差 4、栅格 20),与 C 的 `relPos ±50` 自洽(50 = 30 + 20),`pinTemplate` 坐标无需改。② 追认 SWITCH/LED 单脚模型(代码 `component_library.cpp` 与 `tests/model_test.cpp` 已由队友按第 4 周定稿落地,C 侧确认一致)。③ 补 `docs/interfaces.md`:元件外观 v1 一律"矩形 + 引脚名"(真符号留 v1.1)、未知类型返回空 = 拒绝放置并状态栏提示、命名口径(树显示中文 `displayName`、画布显示 `id`)。无代码改动,仅文档追认。
- **D**:`setSignalCallback` 契约定死(B 局部刷新用)—— OnChange 去重(`setInput`/`step` 统一 `find` 判定,修掉首次拨 Low 漏回调);单次 `step()` 回调上限 1000,超限静默丢弃、整步只打一行警告;SWITCH/LED 与 C 定稿的单脚模型对齐;真实链路随 B 阶段四验证通过(T-10、sim_test PASS);A-D 联调测试 `testSchematicModelIntegration` 已写好(`#if 0` 待构建文件解锁);六个核心函数补设计注释。

### 第 4 周问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

## 第 5 周(2026-10-15 起)

**剩余时间表**(截止 11-12,共 10 周;还剩 4 个完整开发周 + 1 周收尾):

| 周 | 定位 |
| --- | --- |
| 第 5 周(本周) | 撤销 / 重做 + 焊死验收表 → 需求表全绿 |
| 第 6 周 | 剩下的全清:真符号、C3 自定义元件、环路检测、时序元件 |
| 第 7 周起 | **只修不加** —— 后面三周都是 buffer |

> 做得快就把第 6 周的东西往前提:撤销/重做一完就接真符号,别串行等 C 的参数。

### 第 5 周计划

- **A**:① 把 D 那段锁在 `#if 0` 里的 `testSchematicModelIntegration` **挪进 `tests/`** 并挂 CMake 目标 —— 那是它唯一没启用的原因(注释里自己都写着该放 `tests/`,README 第 3 条也是这么定的);顺带修掉它注释里"`addElement` 尚未实现"的过期前提。② **把验收表焊死**:`docs/test-plan.md` 的 T-04 / T-05 还挂着 ⚠️(只差"文件对话框要人工点一次"),给 `NetlistIO` 补一条不走对话框的端到端核对把这两条转 ✅;C-T-06 / C-T-07 是单脚模型之前的旧行,和 C-T-09 / C-T-10 直接打架,删掉。
- **B**:**撤销 / 重做(F-06)** —— 需求表 F-01…F-05 全绿了,这是最后一条。建议**快照式**:每次编辑前把整份 `Schematic` 压进撤销栈,撤销就是 `loadFrom(快照)`;你们的规模下这最省事、也最不容易错(`rebuildNets` 让快照天然自洽,不用写一堆反向操作)。另外 **T-12**(菜单 → 文件对话框,人工点一次)和 `docs/user-manual.md`(演示流程说明)也归你。
- **C**:① **补跑 C 侧那 11 项**并填进 test-plan —— 从第 1 周列上去到现在一场没跑;② **出五个元件真符号的几何参数**(折线 / 圆 / 弧),这是 B 画真符号的前置;③ C3 自定义元件。
- **D**:**环路 / 振荡检测(D3)** —— `step()` 现在只迭代两步,用户连出一个组合环会拿到未定义结果,演示时被随手连出来就尴尬;至少要在状态栏报一句"检测到环路"。另外配合 A 把联调测试解锁。

> 本周关键路径:**B 的撤销 / 重做** —— 需求表最后一条,做完验收表就齐了。
> 另一条线是**把验收表本身焊死**:T-04 / T-05 还挂着 ⚠️,C 侧 11 项没跑过,test-plan 里还留着和单脚模型打架的旧行。

### 第 5 周完成

> 周末回填,没做完就写做到哪了。

- **A**:① **解锁 A-D 联调测试** —— D 那段原先锁在 `src/simulation/simulator.cpp` 的 `#if 0` 里(注释自己都写着该放 `tests/`),现落到 `tests/integration_test.cpp` 并挂 `integration_test` 目标,**22 项自测全过**。比原来多了两项:元件与连线全部走 `SchematicModel` 造数据(不是手写假 Schematic),以及"**删线之后必须重新 load**"这条契约的验证 —— 就是 interfaces.md 提醒的那个坑,现在有测试盯着了。② **顺手清掉 `simulator.cpp` 里两段测试代码** —— 除了那段锁着的,147-207 行还藏着一份与 `tests/main_test.cpp` 重复、且**无人调用**的 `testAndGateTruthTable`;文件从 266 行降到 141 行,连带去掉 3 个只为测试而存在的 include(`<cassert>` `<unordered_map>` `<algorithm>`)。③ **焊死验收表**:T-04 / T-05 的**数据层**转 ✅(`io_test` 61 项覆盖);另立 **T-12** 记"菜单 → 文件对话框"那条 UI 路径 —— 原生模态对话框机器化脚本够不着,写清"待人工点一次",按要求归 B;删掉 C-T-06 / C-T-07 两条与单脚模型打架的旧行;顺手清了 test-plan 的历史遗留行尾空格 / Tab、末尾换行与标题层级。④ `docs/user-manual.md` 一并归 B。验证:全量构建 0 warning,四个测试目标(sim / model 57 / io 61 / integration 22)全过。
- **B**:**撤销 / 重做(F-06)完成 + 手册与验收表收尾** —— ① **快照式撤销/重做**:每次真编辑前把整份 `Schematic` 压入撤销栈(上限 50,超出丢最旧),撤销 = `model.loadFrom(快照)` + `OnDocumentReplaced()`(清选中 / 清开关挡位 / 重建仿真 / 重绘),重做对称;任何新编辑清空重做栈。入栈点只有四处:放置元件、拖动(按下取快照、抬起且确实移动过才入栈)、连线、删除元件或导线;单击选中、Esc 还原、拨开关(只改仿真状态)一律不入栈 —— 后两条由测试盯着;新建 / 打开会清空历史。UI 侧:菜单「编辑 → 撤销 / 重做」+ `Ctrl+Z` / `Ctrl+Y` + 工具栏两个按钮,按可用性置灰(`wxEVT_UPDATE_UI`)。**v1 快照只含原理图、不含仿真状态**,所以撤销后开关回到关闭,状态栏写明这一点(第 5 周定稿决策 1)。② `docs/user-manual.md` 按当前版本重写(演示流程 / 操作说明 / 快捷键 / 已知限制);③ 验收表:新增 **T-13**(撤销 / 重做),T-04 / T-05 的数据层由 `io_test` 覆盖、UI 那段留在 T-12 待人工点一次。验证:`tools/verify_undo.py` 机器化 **16/16**;阶段四端到端 15/15 与 issue 1/4 无回归(性能项受机器负载影响,空载复测 1000 元件 ≈39ms/帧,此前空载 ≈33ms)。
- **C**:
- **D**:`环路 / 振荡检测(D3)` + 配合 A 解锁联调测试 —— ① **结构化组合环检测**:元件当节点、对每条 net 的「输出脚 → 输入脚」建有向边,三色 DFS 找环(与电平无关);`load()` 缓存到 `m_hasLoop`,解决"`step()` 只迭代两步、遇组合环结果未定义"。② **接口** `Simulator::hasCombinationalLoop()`,供 B 在状态栏报"检测到环路"(D 只给 API,不碰 UI)。③ **解锁联调测试(本周 CMakeLists 改动)**:`CMakeLists.txt` 末尾**新增 `integration_test` 目标**,与 model + components + simulation 一起链接 —— `add_executable(integration_test tests/integration_test.cpp src/model/schematic_model.cpp src/components/component_library.cpp src/simulation/simulator.cpp)`;**其余目标(sim / model / io)未动**。④ 顺带删掉合并残留的死测试 `testAndGateTruthTable`(用了 `assert` 而 `<cassert>` 已被 A 删掉,构建不过)。验证:全量 5 目标 0 error,`sim_test` PASS,`integration_test` 22/22。

### 第 5 周问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

-

## 第 6 周(2026-10-22 起)

> 需求表 F-01…F-06 已**全部实现**(F-06 撤销/重做提前完成),功能不再加。本周转入**收尾**:把验收表打绿、整条链路演练一遍。
> 原计划第 6 周"清剩下的功能",现在提前达成,所以真符号与 C3 降为"有余力"。

### 第 6 周计划

- **A**:① **从零走一遍 MVP 链路**并计时(放元件 → 连线 → 保存 → 导出 → 拨开关),跑不通的立刻修,过程补进 `docs/user-manual.md`;② 演示前核对验收表是否全绿 —— 只核对状态列,谁跑的谁填。
- **B**:① **T-12** 人工点一次(菜单 → 文件对话框 → 保存 / 打开 / 导出)—— 验收表最后一项;② 写一版**演示脚本**(照着念的那种),并想好每一步万一不灵的兜底动作;③(有余力)真符号。
- **C**:① **补跑 C 侧那 9 项核对**(`types` / `displayName` / `pinTemplate`)并填进 `docs/test-plan.md` —— 从第 1 周列上去到现在一次没跑;② **真符号几何参数**(折线 / 圆 / 弧)—— 给 B 画与门弧、非门圆圈、开关拨杆、LED 三角。不碰核心逻辑,纯外观,答辩时也好讲;③ C3 自定义元件。
- **D**:① 给 `hasCombinationalLoop()` **补一条测试** —— 现在只有 `load()` 时的警告,测试里一条断言都没有;② 时序元件 / 旋转。

### 第 6 周完成

> 周末回填,没做完就写做到哪了。

- **A**:① **MVP 链路端到端自动化** —— 把 B 的验证台在 Windows 上跑起来了:它本来就是纯 wx 事件注入、没有平台专有代码,所以 **harness 一行没改就能用**;只补了一条 `pixel <x> <y>` 命令(用 `wxDC::GetPixel` 直接读画布颜色),替代 Linux 那边的 `XGetImage`。新增 `tools/verify_mvp.py`,一条命令跑完整条链路并断言:**9/9 通过,整轮 3.6s** —— 放 4 个元件(`4/0/0`)→ 连 3 根线(`4/3/3`)→ 双击两个开关 → 导线变亮绿 `(0,180,0)`、LED 点亮橙色 `(255,170,0)`。CMake 加了 `BUILD_VERIFY_TOOLS` 选项(默认 OFF,不影响别人的构建)。② 排查了一次启动失败:在工具环境里起进程时 PATH 被改写、不含 `C:\mingw64\bin`,exe 找不到 MinGW 的运行时 DLL(`libgcc_s_seh-1.dll` / `libstdc++-6.dll`)就**静默退出,零报错**。本机系统 PATH 里有这个目录,正常双击 / 从 VS Code 启动不受影响,**不需要额外放 DLL**。记这条只是提醒:换一台没装 MinGW 的机器跑演示,会撞同样的静默失败。③ 踩到两个 Windows 坑并写进 `tools/README.md`:wx 的 GUI 程序在 `CreateProcess` 管道下拿不到 stdout(改走临时文件);`subprocess` 用带 `/` 的相对路径会找不到文件(改 `abspath`)。未做:保存 / 打开 / 导出网表要弹原生文件对话框,自动化驱动不了 —— 仍然挂在 T-12(归 B)。④ 修掉 `io_test` 的一个脆弱点:临时文件原本用相对路径 `build/…`,换个工作目录跑就整片 FAIL(在 `/tmp` 和 `build/` 下都复现了);改用系统临时目录,从任意目录跑都是 61/61。
- **B**:
- **C**:
- **D**:

### 第 6 周问题

> 卡住的写这里:哪个文件、什么现象、想找谁。

-
