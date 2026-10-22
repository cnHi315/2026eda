#pragma once
// 电路仿真(负责人 D,对应任务 1、任务 6)
// 交互参考 Logisim:拨开关 -> 信号沿导线传播 -> 输出变化,UI 收到回调后刷新颜色。

#include <functional>
#include <string>
#include <vector>
#include <unordered_map>

#include "contract/data_model.h"

namespace editor {

class Simulator {
public:
    /// 电平变化回调:UI 用 lambda 注册,刷新导线颜色 / LED 亮灭。
    using Callback = std::function<void(const PinRef& pin, SignalLevel level)>;

    void setSignalCallback(Callback cb);

    /// 从原理图建立仿真模型(读取元件类型与连接关系)。
    void load(const Schematic& schematic);

    /// 设置输入元件(开关等)引脚电平。
    void setInput(const std::string& componentId, int pinIndex, SignalLevel level);

    /// 推进一次稳定传播:输入变化后沿拓扑依次更新输出。
    void step();

    /// 查询某引脚当前电平。
    SignalLevel query(const std::string& componentId, int pinIndex) const;

    /// 是否存在组合环路(反馈路径)。load() 时按连接拓扑分析一次并缓存。
    /// UI 可在每次 step() 后查询,为真时在状态栏提示"检测到环路"。
    bool hasCombinationalLoop() const;

private:
    // --- 本周新增内部状态与辅助函数 ---

    /// 缓存传入的原理图数据
    Schematic m_schematic;

    /// UI 回调函数
    Callback m_callback = nullptr;

    /// 记录各引脚当前电平：Key 为 "元件ID#引脚索引"，例如 "U1#0"
    std::unordered_map<std::string, SignalLevel> m_pinStates;

    /// 组合环路检测结果(load() 时更新,经 hasCombinationalLoop() 暴露给 UI)
    bool m_hasLoop = false;

    /// 时序元件(Q 输出)状态:key 为 DFF 等时序元件的 id
    std::unordered_map<std::string, SignalLevel> m_ffStates;

    /// 生成查找 Key
    static std::string makeKey(const std::string& compId, int pinIndex);

    /// 判断元件类型是否为时序元件(DFF/FF/CLOCK 等):
    /// 时序元件的反馈路径不属于组合环路,也走独立的仿真阶段
    static bool isSequentialType(const std::string& type);

    /// 环路检测:load() 时按连接拓扑分析一次并缓存。
    /// 时序元件的反馈路径不属于组合环路,建图时跳过。
    static bool detectCombinationalLoop(const Schematic& schematic);
};

} // namespace editor