# 基本数据模型

> 要求:字段含义、单位、坐标方向、net 怎么生成,都写清楚。以后谁改数据结构,必须更新这份文档。
> 状态:□草稿 ☑评审中 □稳定 | 负责人:A | 最后更新:2026-09-25(每次修改后更新这行)

代码里的唯一定义在 `src/contract/data_model.h`,本文档与其保持一致(改它 = 改契约,先群同步)。

## 结构一览

```cpp
namespace editor {

struct Point { int x = 0; int y = 0; };                // 画布逻辑坐标

enum class SignalLevel { Low, High, Undefined };       // 低/高/未定义
enum class PinDirection { Input, Output };

struct PinDescriptor {                                 // 引脚模板(定义在元件上)
    std::string name;              // "A"、"B"、"Y"
    PinDirection direction;
    Point relPos;                  // 相对元件原点的引脚位置
};

struct Component {                                      // 元件实例
    std::string id;                // "U1"、"SW1"、"LED1",全局唯一
    std::string type;              // "AND"、"OR"、"NOT"、"SWITCH"、"LED"
    std::string name;              // 显示名,可空
    Point pos;                     // 元件原点在画布的位置
    int rotation = 0;              // 0/90/180/270(选做)
    std::vector<PinDescriptor> pins;  // 输入在前,输出在后
};

struct PinRef { std::string componentId; int pinIndex = 0; };  // 引用某元件的第几个引脚

struct Wire { std::string id; PinRef from; PinRef to; };       // 一条导线连两个引脚

struct Net { std::string id; std::string name; std::vector<PinRef> pins; };
// 网络 = 互连引脚的集合(一条输出可以扇出到多个输入)
// 注意:nets 是从 wires 推导出来的结果,不手工维护 —— 见下面「net 是怎么来的」

struct Schematic {
    std::vector<Component> components;
    std::vector<Wire> wires;
    std::vector<Net> nets;
};

} // namespace editor
```

## 约定

- **坐标**:画布逻辑坐标,int,原点在左上角,**y 轴向下**(与 wxDC 一致)。屏幕像素 → 逻辑坐标的缩放由 ui 层负责。
- **id**:字符串,全局唯一,由 SchematicModel 生成(如 "U1"、"net1")。JSON 保存/打开后 **id 保持不变**。
- **net 怎么生成**:`addWire(from, to)` 时,先看两个引脚各自在不在网络里 ——
  - **都不在** → 新建一个网络,把两个引脚放进去
  - **一个在** → 把另一个并入它所在的那个网络
  - **都在同一个** → 什么都不做(网络不变,只是多了一条线)
  - **在两个不同的** → 把这两个网络**合并**成一个(保留前一个的 id,后一个删掉)

- **net 是从 wires 推导出来的**:`wires` 是唯一真相,`nets` 只是从它算出来的结果。
  - **删线**:`removeWire(wireId)` 删掉**一条**导线。注意参数是 `Wire::id`,不是网络 id。
  - **删元件**:`removeElement(id)` 删掉元件,并连带删掉以它引脚为端点的所有导线。
  - 两者做完之后,`nets` 会**整体重算** —— 把剩下的每条线当成一条边,求连通分量,每个分量合成一个网络。不做局部修补。
  - 这样"两个引脚相连 ⟺ 在同一个网络里"永远成立,不会出现"线删了但网络还连着"。
  - 代价:重算后 **net id 会重新分配**。`Wire` 只存 `PinRef`、不存 net id,所以不影响任何已存的数据;但同一个网络重算前后 id 可能不同(只影响 JSON 长什么样)。`net.name` 也会被清掉(v1 里它一直是空的)。
  - 开销与线数成正比,几十根线的规模是微秒级,可以忽略。
  - 只有一个引脚的分组不成网络 —— 网络至少要有两个引脚。
- **拓扑约束**(SchematicModel 内校验):
  - 不允许两个输出引脚直连
  - 同一条线不能重复(两个端点相同,**正反都算**);已经连通的两个引脚之间**可以**再连
- **电平**:仿真只做单比特(v1 不做总线);Undefined = 未初始化/悬空。

## JSON 保存格式示例

```json
{
  "version": 1,
  "components": [
    {
      "id": "U1", "type": "AND", "name": "U1",
      "pos": { "x": 300, "y": 200 }, "rotation": 0,
      "pins": [
        { "name": "A", "direction": "input",  "relPos": { "x": -20, "y": -10 } },
        { "name": "B", "direction": "input",  "relPos": { "x": -20, "y": 10 } },
        { "name": "Y", "direction": "output", "relPos": { "x": 20,  "y": 0 } }
      ]
    }
  ],
  "wires": [
    { "id": "w1",
      "from": { "componentId": "SW1", "pinIndex": 0 },
      "to":   { "componentId": "U1",  "pinIndex": 0 } }
  ],
  "nets": [
    { "id": "net1", "name": "n1",
      "pins": [
        { "componentId": "SW1", "pinIndex": 0 },
        { "componentId": "U1",  "pinIndex": 0 }
      ] }
  ]
}
```

## 变更记录

| 日期 | 修改人 | 内容 |
| --- | --- | --- |
| 2026-09-25 | A | `removeWire` 从「删整个网络」改为「删一条导线」;补充 nets 由 wires 重算的规则 |
