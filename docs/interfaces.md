# 模块接口约定

> 状态:□草稿 ☑评审中 □稳定 | 负责人:A | 最后更新:2026-10-08(每次修改后更新这行)
> 四个类的声明分别在 src/model、src/components、src/io、src/simulation 的头文件里,本文档只做汇总,让每个人不用翻代码就知道别人提供什么。

## SchematicModel(src/model/schematic_model.h,负责人 A)

| 方法 | 说明 |
| --- | --- |
| `const Schematic& data() const` | 只读访问全部数据(UI 渲染用) |
| `void loadFrom(const Schematic&)` | 用现成的原理图替换当前内容(打开文件用);nets 按 wires 重算一遍 |
| `std::string addElement(type, Point)` | 放新元件,返回 id;失败返回空串 |
| `bool removeElement(id)` | 删除元件,连带删掉它引脚上的导线 |
| `bool moveElement(id, Point)` | 移动元件(只改坐标,连接关系不变) |
| `std::string addWire(PinRef from, PinRef to)` | 连线,返回网络 id;自动合并/新建 net(规则见 data-model.md) |
| `bool removeWire(wireId)` | 删掉**一条**导线(不是整个网络) |
| `const Component* findComponent(id) const` | 找不到返回 nullptr |

## ComponentLibrary(src/components/component_library.h,负责人 C)

| 方法 | 说明 |
| --- | --- |
| `std::vector<std::string> types() const` | 如 {"AND","OR","NOT","SWITCH","LED"} |
| `std::string displayName(type) const` | "AND" → "与门" |
| `std::vector<PinDescriptor> pinTemplate(type) const` | 该类型引脚模板 |

### C 侧：pinTemplate 引脚模板对齐表(任务 3)

> 以下 relPos 为相对元件原点的逻辑坐标,UI 画符号与命中检测均以此为准。
> 坐标来源:B 画布临时端点,如后续调整需同步更新此表。

| 类型 | 显示名 | 引脚名 | 方向 | relPos (x, y) |
| ------ | -------- | -------- | ------ | --------------- |
| AND | 与门 | A | Input | (-50, -10) |
| AND | 与门 | B | Input | (-50, 10) |
| AND | 与门 | Y | Output | (50, 0) |
| OR | 或门 | A | Input | (-50, -10) |
| OR | 或门 | B | Input | (-50, 10) |
| OR | 或门 | Y | Output | (50, 0) |
| NOT | 非门 | A | Input | (-50, 0) |
| NOT | 非门 | Y | Output | (50, 0) |
| SWITCH | 开关 | Y | Output | (50, 0) |
| LED | LED | A | Input | (-50, 0) |

> **第 4 周定稿(2026-10-08)**:SWITCH 收敛成 **1 个引脚(输出 Y)**、LED 收敛成 **1 个引脚(输入 A)** ——
> 开关是源、LED 是汇,物理上也说得通。这样做是为了让 `setInput(id, 0, …)` 落在输出脚上(两脚模型时 0 号是输入脚,
> 电平进不了网络,"拨开关"是死的)。连带改动:`src/components/component_library.cpp`(引脚模板)、
> `tests/model_test.cpp`(引脚下标与"输出直连"用例)。**待 C 追认**。

### B 侧:画布几何参数对齐表(第 4 周由 B 定稿,C/A 追认)

> 这组参数原先悬在 UI 代码里、又卡着 C 的符号尺寸,第 4 周改由 B 一次定死并公开。
> 它与上面的 `relPos` 自洽:**±50 = 30(半宽) + 20(引脚可视长度)**。
> 单位都是逻辑坐标(wx 在 HiDPI 下已按 DIP,不要再套 `FromDIP()`)。

| 参数 | 取值 | 说明 |
| --- | --- | --- |
| 元件包围盒 | **60 × 40** | 五种类型统一,不随类型变化(v1) |
| 引脚可视长度 | **20** | 从包围盒边框画到 `relPos` 端点 |
| 元件命中盒 | 包围盒本身(60 × 40) | 不额外外扩 |
| 引脚命中半径 | **8** | 命中优先级:引脚 → 元件 → 导线 |
| 导线命中容差 | **4** | 点到线段距离 |
| 栅格 / 吸附步长 | **20** | 细网格 20、每 5 格一条粗线;拖拽与放置都吸附到 20 的整数倍 |
| 线宽 | 网格 1 / 元件外框 2 / 导线 2 / 选中导线 4 | |
| 选中框 | 包围盒外扩 5 | 蓝色实线 |
| 引脚端点圆点 | r=2 常态 / r=5 高亮 | 高亮 = 悬停 / 选中 / 吸附 |
| 引脚名字号 | 8pt | 画在端点右上 (x+5, y-16) |

落地位置(唯一出口,改这里就够):`CanvasPanel::SizeOf(type)` 与常量 `kPinHitR / kWireHitR / kGridStep / kSymW / kSymH / kPinLen`。

### UI 交互约定(第 4 周定稿,负责人 B)

| 操作 | 手势 | 说明 |
| --- | --- | --- |
| 选中元件 / 引脚 / 导线 | 左键单击 | 单击不产生位移;点空白清空选中 |
| 移动元件 | 左键拖动 | 实时跟随,抬起吸附到 20 栅格;Esc 取消并还原 |
| 连线 | 从引脚按下 → 拖到目标引脚 → 松开 | 橡皮筋预览;非法连线在状态栏给原因 |
| **拨动开关** | **左键双击** | 在 SWITCH 上双击翻转电平(左键单击已被"选中/拖动"占用) |
| 取消当前操作 | Esc / 右键 | 拖拽中取消会还原位置 |

## NetlistIO(src/io/netlist_io.h,负责人 A)

| 方法 | 说明 |
| --- | --- |
| `bool save(const Schematic&, path)` | 存 JSON |
| `bool load(Schematic&, path)` | 读 JSON(先清空再重建),失败时原理图不变 |
| `bool exportNetlist(const Schematic&, path)` | 导出文本网表 |

## Simulator(src/simulation/simulator.h,负责人 D)

| 方法 | 说明 |
| --- | --- |
| `void setSignalCallback(Callback)` | 注册电平变化回调(UI 用它刷新颜色) |
| `void load(const Schematic&)` | 从原理图建立仿真模型 |
| `void setInput(componentId, pinIndex, SignalLevel)` | 拨开关 |
| `void step()` | 传播一步直到稳定 |
| `SignalLevel query(componentId, pinIndex) const` | 查询电平 |

## 调用示例(UI 视角)

```cpp
editor::SchematicModel model;
editor::ComponentLibrary lib;

std::string id = model.addElement("AND", {300, 200});  // 放一个与门
const editor::Schematic& s = model.data();               // 渲染用

editor::NetlistIO io;
io.save(s, "demo.json");                                 // 保存

editor::Simulator sim;
sim.setSignalCallback([](const editor::PinRef& pin, editor::SignalLevel lv) {
    // 刷新对应导线颜色 / LED 亮灭
});
sim.load(s);
sim.setInput("SW1", 0, editor::SignalLevel::High);       // 打开开关
sim.step();                                              // 传播
```

**改完数据要重载仿真**:`Simulator::load()` 存的是副本,之后再改 model 它不知道。UI 每次增删元件/导线后要重调 `sim.load(model.data())`,否则线删了 LED 还亮着。

> 约定:改任何签名前先在群里说一声,并更新本文档。
