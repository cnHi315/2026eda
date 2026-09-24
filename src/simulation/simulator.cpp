#include "simulation/simulator.h"
#include <unordered_map>

// TODO(D): 每完成一项对照 docs/test-plan.md 的 T-06/T-07 核对。
// 建议顺序:组合逻辑传播 -> 事件队列 -> (选做)环路检测、时序元件。
// 单元自测放在 tests/ 下,不要写进本文件。

namespace editor {
//新增的辅助函数（Simulator 的私有静态成员）
std::string Simulator::makeKey(const std::string& compId, int pinIndex) {
    return compId + "#" + std::to_string(pinIndex);
}
//改动原有函数
void Simulator::setSignalCallback(Callback cb) {
    m_callback = cb;
}

void Simulator::load(const Schematic& schematic) {
    m_schematic = schematic;
    m_pinStates.clear();
}

void Simulator::setInput(const std::string& componentId, int pinIndex, SignalLevel level) {
    m_pinStates[makeKey(componentId, pinIndex)] = level;
    if (m_callback) {
        m_callback({componentId, pinIndex}, level);
    }
}

void Simulator::step() {
    // 组合逻辑两步迭代传播
    for (int iter = 0; iter < 2; ++iter) {
        // 1. 沿网络传播：将有效电平广播给同一网络下的所有引脚
        for (const auto& net : m_schematic.nets) {
            SignalLevel netLevel = SignalLevel::Undefined;
            for (const auto& pin : net.pins) {
                SignalLevel lvl = query(pin.componentId, pin.pinIndex);
                if (lvl != SignalLevel::Undefined) {
                    netLevel = lvl;
                    break;
                }
            }
            if (netLevel != SignalLevel::Undefined) {
                for (const auto& pin : net.pins) {
                    m_pinStates[makeKey(pin.componentId, pin.pinIndex)] = netLevel;
                }
            }
        }

        // 2. 与门逻辑计算
        for (const auto& comp : m_schematic.components) {
            if (comp.type == "AND") {
                SignalLevel in0 = query(comp.id, 0);
                SignalLevel in1 = query(comp.id, 1);

                if (in0 == SignalLevel::High && in1 == SignalLevel::High) {
                    m_pinStates[makeKey(comp.id, 2)] = SignalLevel::High;
                } else if (in0 == SignalLevel::Low || in1 == SignalLevel::Low) {
                    m_pinStates[makeKey(comp.id, 2)] = SignalLevel::Low;
                }
            }
        }
    }
}

SignalLevel Simulator::query(const std::string& componentId, int pinIndex) const {
    auto it = m_pinStates.find(makeKey(componentId, pinIndex));
    if (it != m_pinStates.end()) {
        return it->second;
    }
    return SignalLevel::Undefined;
}

} // namespace editor
