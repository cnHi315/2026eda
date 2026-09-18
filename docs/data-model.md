# 基本数据模型

> 要求:字段含义、单位、坐标方向、net 怎么生成,都写清楚。以后谁改数据结构,必须更新这份文档。
> 状态:□草稿 ☑评审中 □稳定 | 负责人:A | 最后更新:2026-09-10(每次修改后更新这行)

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

  删除:`removeWire(netId)` 删除整个网络。
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
| ____ | ____ | ____ |
