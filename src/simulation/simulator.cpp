#include "simulation/simulator.h"
#include <iostream>

namespace editor {

std::string Simulator::makeKey(const std::string& compId, int pinIndex) {
    return compId + "#" + std::to_string(pinIndex);
}

void Simulator::setSignalCallback(Callback cb) {
    m_callback = cb;
}

static bool detectCombinationalLoop(const Schematic& schematic) {
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i < schematic.components.size(); ++i) {
        index[schematic.components[i].id] = i;
    }

    auto isOutput = [&schematic, &index](const PinRef& p) -> bool {
        auto it = index.find(p.componentId);
        if (it == index.end()) {
            return false;
        }
        const Component& c = schematic.components[it->second];
        if (p.pinIndex < 0 || p.pinIndex >= static_cast<int>(c.pins.size())) {
            return false;
        }
        return c.pins[p.pinIndex].direction == PinDirection::Output;
    };

    std::vector<std::vector<size_t>> adj(schematic.components.size());
    for (const auto& net : schematic.nets) {
        for (const auto& from : net.pins) {
            if (!isOutput(from)) {
                continue;
            }
            const auto fi = index.find(from.componentId);
            if (fi == index.end()) {
                continue;
            }
            for (const auto& to : net.pins) {
                if (isOutput(to)) {
                    continue;
                }
                const auto ti = index.find(to.componentId);
                if (ti == index.end()) {
                    continue;
                }
                adj[fi->second].push_back(ti->second);
            }
        }
    }

    enum Color { White, Gray, Black };
    std::vector<int> color(schematic.components.size(), White);
    std::function<bool(size_t)> dfs = [&adj, &color, &dfs](size_t u) -> bool {
        color[u] = Gray;
        for (size_t v : adj[u]) {
            if (color[v] == Gray) {
                return true;
            }
            if (color[v] == White && dfs(v)) {
                return true;
            }
        }
        color[u] = Black;
        return false;
    };
    for (size_t i = 0; i < adj.size(); ++i) {
        if (color[i] == White && dfs(i)) {
            return true;
        }
    }
    return false;
}

void Simulator::load(const Schematic& schematic) {
    m_schematic = schematic;
    m_pinStates.clear();
    m_hasLoop = detectCombinationalLoop(m_schematic);
    if (m_hasLoop) {
        std::cerr << "[Warning] 检测到环路:电路存在组合反馈,仿真结果可能不稳定" << std::endl;
    }
}

bool Simulator::hasCombinationalLoop() const {
    return m_hasLoop;
}

void Simulator::setInput(const std::string& componentId, int pinIndex, SignalLevel level) {
    std::string key = makeKey(componentId, pinIndex);
    auto it = m_pinStates.find(key);
    if (it == m_pinStates.end() || it->second != level) {
        m_pinStates[key] = level;
        if (m_callback) {
            m_callback({componentId, pinIndex}, level);
        }
    }
}

void Simulator::step() {
    int stepCallbackCount = 0;
    bool limitWarned = false;
    const int kMaxCallbacksPerStep = 1000;

    auto updatePinState = [this, &stepCallbackCount, &limitWarned, kMaxCallbacksPerStep](
            const std::string& compId, int pinIdx, SignalLevel newLvl) {
        std::string key = makeKey(compId, pinIdx);
        auto it = m_pinStates.find(key);
        if (it == m_pinStates.end() || it->second != newLvl) {
            m_pinStates[key] = newLvl;
            if (m_callback) {
                if (stepCallbackCount < kMaxCallbacksPerStep) {
                    ++stepCallbackCount;
                    m_callback({compId, pinIdx}, newLvl);
                } else if (!limitWarned) {
                    limitWarned = true;
                    std::cerr << "[Warning] Simulator::step() 达到单步回调上限 ("
                              << kMaxCallbacksPerStep << ")，触发限流保护！" << std::endl;
                }
            }
        }
    };

    for (int iter = 0; iter < 2; ++iter) {
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

}
