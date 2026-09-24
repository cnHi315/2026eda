#include "simulation/simulator.h"
#include <iostream>
#include <cassert>
#include <unordered_map>

// TODO(D): 每完成一项对照 docs/test-plan.md 的 T-06/T-07 核对。
// 建议顺序:组合逻辑传播 -> 事件队列 -> (选做)环路检测、时序元件。

namespace editor {

// 新增的辅助函数（Simulator 的私有静态成员）
std::string Simulator::makeKey(const std::string& compId, int pinIndex) {
    return compId + "#" + std::to_string(pinIndex);
}

void Simulator::setSignalCallback(Callback cb) {
    m_callback = cb;
}

void Simulator::load(const Schematic& schematic) {
    m_schematic = schematic;
    m_pinStates.clear();
}

void Simulator::setInput(const std::string& componentId, int pinIndex, SignalLevel level) {
    std::string key = makeKey(componentId, pinIndex);
    // 优化：仅当电平发生实际改变时更新并触发回调（响应前端 OnChange 规范）
    if (m_pinStates[key] != level) {
        m_pinStates[key] = level;
        if (m_callback) {
            m_callback({componentId, pinIndex}, level);
        }
    }
}

void Simulator::step() {
    // 内部私有 Lambda：安全更新引脚状态，若电平有变则自动触发 UI 回调
    // 这样不仅手动 input 能触发回调，step 仿真推演时网线传播和门计算也能实时通知前端！
    auto updatePinState = [this](const std::string& compId, int pinIdx, SignalLevel newLvl) {
        std::string key = makeKey(compId, pinIdx);
        if (m_pinStates[key] != newLvl) {
            m_pinStates[key] = newLvl;
            if (m_callback) {
                m_callback({compId, pinIdx}, newLvl);
            }
        }
    };

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
                    updatePinState(pin.componentId, pin.pinIndex, netLevel);
                }
            }
        }

        // 2. 逻辑门计算 (支持 AND, OR, NOT) 并通过 updatePinState 广播
        for (const auto& comp : m_schematic.components) {
            if (comp.type == "AND") {
                SignalLevel in0 = query(comp.id, 0);
                SignalLevel in1 = query(comp.id, 1);

                SignalLevel outLvl = SignalLevel::Undefined;
                if (in0 == SignalLevel::High && in1 == SignalLevel::High) {
                    outLvl = SignalLevel::High;
                } else if (in0 == SignalLevel::Low || in1 == SignalLevel::Low) {
                    outLvl = SignalLevel::Low;
                }
                if (outLvl != SignalLevel::Undefined) {
                    updatePinState(comp.id, 2, outLvl);
                }
            } 
            else if (comp.type == "OR") {
                SignalLevel in0 = query(comp.id, 0);
                SignalLevel in1 = query(comp.id, 1);

                SignalLevel outLvl = SignalLevel::Undefined;
                if (in0 == SignalLevel::High || in1 == SignalLevel::High) {
                    outLvl = SignalLevel::High;
                } else if (in0 == SignalLevel::Low && in1 == SignalLevel::Low) {
                    outLvl = SignalLevel::Low;
                }
                if (outLvl != SignalLevel::Undefined) {
                    updatePinState(comp.id, 2, outLvl);
                }
            }
            else if (comp.type == "NOT") {
                SignalLevel in0 = query(comp.id, 0);
                SignalLevel outLvl = SignalLevel::Undefined;
                if (in0 == SignalLevel::High) {
                    outLvl = SignalLevel::Low;
                } else if (in0 == SignalLevel::Low) {
                    outLvl = SignalLevel::High;
                }
                if (outLvl != SignalLevel::Undefined) {
                    updatePinState(comp.id, 1, outLvl);
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

// 测试函数：独立自测 AND、OR、NOT 真值表并完整验证 UI 回调机制
void testAndGateTruthTable() {
    std::cout << "\n>>> 开始运行 D 任务: 仿真引擎逻辑与回调全功能自测 <<<" << std::endl;

    // -------------------------------------------------------------------
    // 1. 手工构建电路:两个开关 -> 与门 -> LED,3 条网络连线
    // -------------------------------------------------------------------
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
    sch.nets = {
        {"netA", "A", {{"SW_A", 0}, {"U_AND", 0}}},
        {"netB", "B", {{"SW_B", 0}, {"U_AND", 1}}},
        {"netY", "Y", {{"U_AND", 2}, {"LED_OUT", 0}}}
    };

    // -------------------------------------------------------------------
    // 2. 加载到仿真引擎,挂载 setSignalCallback 回调
    // -------------------------------------------------------------------
    Simulator sim;
    sim.load(sch);

    int callbackFireCount = 0;
    sim.setSignalCallback([&callbackFireCount](editor::PinRef pin, SignalLevel lvl) {
        ++callbackFireCount;
        std::cout << "  📢 [UI回调广播]: 元件 " << pin.componentId << "#" << pin.pinIndex
                  << " -> " << (lvl == SignalLevel::High ? "高(1)" : "低(0)") << std::endl;
    });

    // -------------------------------------------------------------------
    // 3. 与门真值表四组输入验证
    // -------------------------------------------------------------------
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
        std::cout << "AND 测试: A=" << (tc.a == SignalLevel::High ? "1" : "0")
                  << ", B=" << (tc.b == SignalLevel::High ? "1" : "0")
                  << " => LED=" << (out == SignalLevel::High ? "1" : "0")
                  << " (期望=" << (tc.expected == SignalLevel::High ? "1" : "0") << ") -> ";
        assert(out == tc.expected);
        std::cout << "[PASS]" << std::endl;
    }

    // 回调必须被触发过
    assert(callbackFireCount > 0);
    std::cout << ">>> [PASS] 与门真值表 + setSignalCallback 回调验证通过！(总共触发回调 "
              << callbackFireCount << " 次) <<<" << std::endl;

    // -------------------------------------------------------------------
    // 4. 或门(OR)真值表验证
    // -------------------------------------------------------------------
    Schematic orSch;
    Component orGate{"U_OR", "OR", "OR", {0, 0}, 0, {
        {"A", PinDirection::Input, {0, 0}},
        {"B", PinDirection::Input, {0, 10}},
        {"Y", PinDirection::Output, {20, 5}}
    }};
    Component led2{"LED_OR", "LED", "LED", {0, 0}, 0, {{"IN", PinDirection::Input, {0, 0}}}};
    orSch.components = {swA, swB, orGate, led2};
    orSch.nets = {
        {"netA", "A", {{"SW_A", 0}, {"U_OR", 0}}},
        {"netB", "B", {{"SW_B", 0}, {"U_OR", 1}}},
        {"netY", "Y", {{"U_OR", 2}, {"LED_OR", 0}}}
    };
    Simulator orSim;
    orSim.load(orSch);

    SignalLevel orCases[][3] = {
        {SignalLevel::Low,  SignalLevel::Low,  SignalLevel::Low},   // 0 | 0 = 0
        {SignalLevel::Low,  SignalLevel::High, SignalLevel::High},  // 0 | 1 = 1
        {SignalLevel::High, SignalLevel::Low,  SignalLevel::High},  // 1 | 0 = 1
        {SignalLevel::High, SignalLevel::High, SignalLevel::High}   // 1 | 1 = 1
    };
    for (const auto& tc : orCases) {
        orSim.setInput("SW_A", 0, tc[0]);
        orSim.setInput("SW_B", 0, tc[1]);
        orSim.step();
        SignalLevel out = orSim.query("LED_OR", 0);
        std::cout << "OR  测试: A=" << (tc[0] == SignalLevel::High ? "1" : "0")
                  << ", B=" << (tc[1] == SignalLevel::High ? "1" : "0")
                  << " => LED=" << (out == SignalLevel::High ? "1" : "0")
                  << " (期望=" << (tc[2] == SignalLevel::High ? "1" : "0") << ") -> ";
        assert(out == tc[2]);
        std::cout << "[PASS]" << std::endl;
    }

    // -------------------------------------------------------------------
    // 5. 非门(NOT)真值表验证
    // -------------------------------------------------------------------
    Schematic notSch;
    Component notGate{"U_NOT", "NOT", "NOT", {0, 0}, 0, {
        {"A", PinDirection::Input, {0, 0}},
        {"Y", PinDirection::Output, {20, 5}}
    }};
    Component led3{"LED_NOT", "LED", "LED", {0, 0}, 0, {{"IN", PinDirection::Input, {0, 0}}}};
    notSch.components = {swA, notGate, led3};
    notSch.nets = {
        {"netA", "A", {{"SW_A", 0}, {"U_NOT", 0}}},
        {"netY", "Y", {{"U_NOT", 1}, {"LED_NOT", 0}}}
    };
    Simulator notSim;
    notSim.load(notSch);

    SignalLevel notCases[][2] = {
        {SignalLevel::Low,  SignalLevel::High},  // !0 = 1
        {SignalLevel::High, SignalLevel::Low},   // !1 = 0
    };
    for (const auto& tc : notCases) {
        notSim.setInput("SW_A", 0, tc[0]);
        notSim.step();
        SignalLevel out = notSim.query("LED_NOT", 0);
        std::cout << "NOT 测试: A=" << (tc[0] == SignalLevel::High ? "1" : "0")
                  << " => LED=" << (out == SignalLevel::High ? "1" : "0")
                  << " (期望=" << (tc[1] == SignalLevel::High ? "1" : "0") << ") -> ";
        assert(out == tc[1]);
        std::cout << "[PASS]" << std::endl;
    }

    std::cout << ">>> [PASS] AND/OR/NOT 真值表 + 动态回调全功能验证全部通过！<<<\n" << std::endl;
}

}