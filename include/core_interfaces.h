#pragma once

// ============================================================================
// core_interfaces.h — core 层暴露给 ui 层的契约接口（Contract-First Development）
//
// 本文件是 UI 与 Core 之间唯一的耦合边界，规则如下：
//   1. 仅包含纯虚接口（class I...）与 POD 数据描述，禁止任何实现（无 .cpp）。
//   2. 禁止包含任何 wxWidgets 类型：坐标/位宽一律用 POD（int / std::uint32_t）。
//   3. 坐标单位为「画布逻辑坐标」(int)，由 ui 层负责缩放与像素换算。
//   4. core 组员在自己的模块内实现这些接口；ui 组员依赖本头文件进行开发。
// ============================================================================

#include <cstdint>
#include <string>
#include <vector>

namespace editor::core {

// ---------------------------------------------------------------------------
// 基础标识与枚举
// ---------------------------------------------------------------------------

/// 电路元素的唯一标识（放置到电路中的每个实例各一个）。
using ElementId = std::uint32_t;

/// 网表网络（若干互连引脚）的唯一标识。
using NetId = std::uint32_t;

/// 引脚方向。
enum class PinDirection {
    Input,   ///< 输入引脚
    Output,  ///< 输出引脚
};

/// 逻辑电平（仿真结果）。
enum class SignalLevel {
    Low,        ///< 低电平（0）
    High,       ///< 高电平（1）
    Undefined,  ///< 未定义 / 高阻 / 未初始化
};

/// 画布逻辑坐标（POD，禁止使用 wxPoint）。
struct Point {
    int x = 0;
    int y = 0;
};

/// 引脚描述。
struct PinDescriptor {
    PinDirection direction = PinDirection::Input;
    int bitWidth = 1;  ///< 位宽：1 = 单线，>1 = 总线
};

/// 引脚引用：用于连线（addWire）与观察者回调，唯一定位一个引脚。
/// 约定：pinIndex 为所属元件 pins() 列表中的下标，输入引脚在前、输出引脚在后。
struct PinRef {
    ElementId element = 0;
    int pinIndex = 0;
};

/// 导线/网络描述：一条导线连接两个引脚端点。
struct WireDescriptor {
    NetId net = 0;
    PinRef from;
    PinRef to;
};

// ---------------------------------------------------------------------------
// 接口 1：电路元素（只读描述）
// ---------------------------------------------------------------------------

/// 表示电路中的一个元件实例，暴露 UI 渲染与命中检测所需的只读信息。
/// 由 core 层实现（对应 Role A）。返回的引用/指针均为非拥有，元素存续期内有效。
class ICircuitElement {
public:
    virtual ~ICircuitElement() = default;

    /// 元素唯一标识。
    virtual ElementId id() const = 0;

    /// 元件类型标识（如 "AND" / "OR" / "NOT" / "CLOCK" / "INPUT" / "OUTPUT"）。
    virtual std::string type() const = 0;

    /// 画布逻辑坐标。
    virtual Point position() const = 0;

    /// 全部引脚（输入在前、输出在后）。返回稳定引用，调用期间有效。
    virtual const std::vector<PinDescriptor>& pins() const = 0;
};

// ---------------------------------------------------------------------------
// 接口 2：网表模型（UI 可调用的命令 + 只读查询）
// ---------------------------------------------------------------------------

/// 网表模型：UI 通过本接口修改与查询电路结构。
/// 由 core 层实现（对应 Role A）；所有写命令应保证内部一致性（如拓扑约束）。
class INetlistModel {
public:
    virtual ~INetlistModel() = default;

    // ---- 写命令 ----

    /// 放置一个指定类型的新元件到坐标 position，成功返回新元素 id（0 表示失败）。
    virtual ElementId addElement(const std::string& type, Point position) = 0;

    /// 移除指定元素（并断开其相连导线），成功返回 true。
    virtual bool removeElement(ElementId id) = 0;

    /// 连接两个引脚（添加一条导线/网络），成功返回网络 id（0 表示失败）。
    virtual NetId addWire(PinRef from, PinRef to) = 0;

    /// 断开指定网络，成功返回 true。
    virtual bool removeWire(NetId net) = 0;

    /// 移动元素到新坐标，成功返回 true。
    virtual bool moveElement(ElementId id, Point position) = 0;

    // ---- 只读查询（供 UI 渲染 / 命中检测） ----

    /// 按 id 查找元素；不存在返回 nullptr。
    virtual const ICircuitElement* findElement(ElementId id) const = 0;

    /// 全部元素 id（用于迭代渲染）。
    virtual std::vector<ElementId> elementIds() const = 0;

    /// 全部导线（用于迭代渲染连线）。
    virtual std::vector<WireDescriptor> wires() const = 0;
};

// ---------------------------------------------------------------------------
// 接口 3：仿真观察者（观察者模式回调）
// ---------------------------------------------------------------------------

/// 由 ui 层实现，注册到 core 的仿真引擎；某引脚信号状态变化时引擎回调本接口。
/// 注册/注销方法由仿真引擎（Role B）提供，不在此接口内。
class ISimulationObserver {
public:
    virtual ~ISimulationObserver() = default;

    /// 某元件某引脚的电平发生变化。
    virtual void onSignalChanged(const PinRef& pin, SignalLevel level) = 0;
};

} // namespace editor::core