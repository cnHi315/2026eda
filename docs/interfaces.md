# 模块接口约定

> 状态:□草稿 ☑评审中 □稳定 | 负责人:A | 最后更新:2026-09-10(每次修改后更新这行)
> 四个类的声明分别在 src/model、src/components、src/io、src/simulation 的头文件里,本文档只做汇总,让每个人不用翻代码就知道别人提供什么。

## SchematicModel(src/model/schematic_model.h,负责人 A)

| 方法 | 说明 |
| --- | --- |
| `const Schematic& data() const` | 只读访问全部数据(UI 渲染用) |
| `std::string addElement(type, Point)` | 放新元件,返回 id;失败返回空串 |
| `bool removeElement(id)` | 删除元件并断开其导线 |
| `bool moveElement(id, Point)` | 移动元件 |
| `std::string addWire(PinRef from, PinRef to)` | 连线,返回网络 id;自动合并/新建 net(规则见 data-model.md) |
| `bool removeWire(netId)` | 断开整个网络 |
| `const Component* findComponent(id) const` | 找不到返回 nullptr |

## ComponentLibrary(src/components/component_library.h,负责人 C)

| 方法 | 说明 |
| --- | --- |
| `std::vector<std::string> types() const` | 如 {"AND","OR","NOT","SWITCH","LED"} |
| `std::string displayName(type) const` | "AND" → "与门" |
| `std::vector<PinDescriptor> pinTemplate(type) const` | 该类型引脚模板 |

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

> 约定:改任何签名前先在群里说一声,并更新本文档。
