#include "simulation/simulator.h"
#include <iostream>
#include <cassert>
#include <unordered_map>

// TODO(D): 每完成一项对照 docs/test-plan.md 的 T-06/T-07 核对。
// 建议顺序:组合逻辑传播 -> 事件队列 -> (选做)环路检测、时序元件。

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
    //测试函数
void testAndGateTruthTable() {
    std::cout << "\n>>> 开始运行 D 任务: 与门真值表自测 <<<" << std::endl;

    // 1. 手写构建假电路数据
    Schematic sch;
    Component swA{"SW_A", "SWITCH", "SW_A", {0, 0}, 0, {{"OUT", PinDirection::Output, {0, 0}}}};
    Component swB{"SW_B", "SWITCH", "SW_B", {0, 0}, 0, {{"OUT", PinDirection::Output, {0, 0}}}};
    Component andGate{"U_AND", "AND", "AND", {0, 0}, 0, {
        {"A", PinDirection::Input, {0, 0}},
        {"B", PinDirection::Input, {0, 10}},
        {"Y", PinDirection::Output, {20, 5}}
    }};
    Component led{"LED_OUT", "LED", "LED", {0, 0}, 0, {{"IN", PinDirection::Input, {0, 0}}}};

    sch.components = {swA, swB, andGate, led};

    // 2. 构造网络连接
    Net netA{"netA", "A", {{"SW_A", 0}, {"U_AND", 0}}};
    Net netB{"netB", "B", {{"SW_B", 0}, {"U_AND", 1}}};
    Net netY{"netY", "Y", {{"U_AND", 2}, {"LED_OUT", 0}}};
    sch.nets = {netA, netB, netY};

    // 3. 仿真与门真值表四组输入
    Simulator sim;
    sim.load(sch);

    struct TruthTableCase {
        SignalLevel a;
        SignalLevel b;
        SignalLevel expected;
    } cases[] = {
        {SignalLevel::Low,  SignalLevel::Low,  SignalLevel::Low},   // 0 & 0 = 0
        {SignalLevel::Low,  SignalLevel::High, SignalLevel::Low},   // 0 & 1 = 0
        {SignalLevel::High, SignalLevel::Low,  SignalLevel::Low},   // 1 & 0 = 0
        {SignalLevel::High, SignalLevel::High, SignalLevel::High}   // 1 & 1 = 1
    };

    for (const auto& tc : cases) {
        sim.setInput("SW_A", 0, tc.a);
        sim.setInput("SW_B", 0, tc.b);
        sim.step();

        SignalLevel out = sim.query("LED_OUT", 0);
        std::cout << "测试: A=" << (tc.a == SignalLevel::High ? "1" : "0")
                  << ", B=" << (tc.b == SignalLevel::High ? "1" : "0")
                  << " => LED=" << (out == SignalLevel::High ? "1" : "0")
                  << " (期望=" << (tc.expected == SignalLevel::High ? "1" : "0") << ") -> ";

        assert(out == tc.expected);
        std::cout << "[PASS]" << std::endl;
    }

    std::cout << ">>> [PASS] 与门真值表全部验证通过！<<<\n" << std::endl;
}
} // namespace editor
