#include "simulation/simulator.h"

// TODO(D): 每完成一项对照 docs/test-plan.md 的 T-06/T-07 核对。
// 建议顺序:组合逻辑传播 -> 事件队列 -> (选做)环路检测、时序元件。

namespace editor {

void Simulator::setSignalCallback(Callback cb) {
    (void)cb;  // TODO(D)
}

void Simulator::load(const Schematic& schematic) {
    (void)schematic;  // TODO(D)
}

void Simulator::setInput(const std::string& componentId, int pinIndex, SignalLevel level) {
    (void)componentId;
    (void)pinIndex;
    (void)level;  // TODO(D)
}

void Simulator::step() {
    // TODO(D)
}

SignalLevel Simulator::query(const std::string& componentId, int pinIndex) const {
    (void)componentId;
    (void)pinIndex;
    return SignalLevel::Undefined;  // TODO(D)
}

} // namespace editor
