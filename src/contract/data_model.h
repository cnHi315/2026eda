#pragma once
// ============================================================================
// data_model.h —— 全项目唯一契约(数据结构)
//
// 规则:
//   1. 只放结构体和枚举,不放函数实现。
//   2. 任何人改这个文件,必须先在群里说一声,并同步更新
//      docs/data-model.md 与 docs/interfaces.md。
//   3. 不包含任何 wx 头文件。
//
// 约定:
//   - 坐标:画布逻辑坐标(int),原点在左上角,y 轴向下(与 wxDC 一致),
//     屏幕像素与逻辑坐标的换算由 ui 层负责。
//   - id:字符串,全局唯一,由 SchematicModel 生成。
//     元件 id 与导线 id 一经分配就不再变;网络 id 会在删线/删元件后重新分配
//     (nets 由 wires 推导,见 docs/data-model.md)。
//   - 见 docs/data-model.md。
// ============================================================================

#include <string>
#include <vector>

namespace editor {

/// 画布逻辑坐标
struct Point {
    int x = 0;
    int y = 0;
};

/// 逻辑电平(仿真结果)
enum class SignalLevel {
    Low,        ///< 低电平(0)
    High,       ///< 高电平(1)
    Undefined,  ///< 未定义 / 未初始化
};

/// 引脚方向
enum class PinDirection {
    Input,   ///< 输入引脚
    Output,  ///< 输出引脚
};

/// 引脚(定义在元件上的模板)
struct PinDescriptor {
    std::string name;                        ///< 引脚名,如 "A"、"B"、"Y"
    PinDirection direction = PinDirection::Input;
    Point relPos;                            ///< 相对元件原点的引脚位置(画线端点/命中检测用)
};

/// 元件实例
struct Component {
    std::string id;                          ///< "U1"、"SW1"、"LED1",全局唯一
    std::string type;                        ///< "AND"、"OR"、"NOT"、"SWITCH"、"LED"
    std::string name;                        ///< 显示名,可空
    Point pos;                               ///< 元件原点在画布的位置
    int rotation = 0;                        ///< 0/90/180/270,选做
    std::vector<PinDescriptor> pins;         ///< 输入在前,输出在后
};

/// 引脚引用:哪个元件的第几个引脚
struct PinRef {
    std::string componentId;
    int pinIndex = 0;
};

/// 一条导线,连接两个引脚
struct Wire {
    std::string id;
    PinRef from;
    PinRef to;
};

/// 网络:若干互连引脚的集合(一条输出可以扇出到多个输入)
/// 注意:这是从 Schematic::wires 推导出来的结果,不手工维护 —— 删线/删元件后会整体重算。
struct Net {
    std::string id;
    std::string name;                        ///< 导出网表时的网络名,可空
    std::vector<PinRef> pins;
};

/// 原理图:一切数据的根
struct Schematic {
    std::vector<Component> components;
    std::vector<Wire> wires;
    std::vector<Net> nets;
};

} // namespace editor
