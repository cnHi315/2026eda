#include "simulation/simulator.h"
#include <iostream>

namespace editor {

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
    bool limitWarned = false;  // 熔断警告只输出一次，避免刷屏（之前每超一次都打一行，console 被刷爆）
    const int kMaxCallbacksPerStep = 1000; // 安全上限阀值：B 据此预算 dirty rect 数量

    // 内部私有 Lambda：安全更新引脚状态，带去重、回调广播与上限保护。
    // 整个 step() 内的所有引脚更新都走这个 lambda，确保传播链上同一引脚最多回调一次。
    // 捕获说明：counter / warned 用引用捕获（step 栈上变量），阀值用值捕获（const 常量）。
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
                    // 超过单步回调上限时进行熔断保护，防止前端 UI 渲染死锁。
                    // 注意用 else if 而非独立 if：超限后继续静默丢弃后续回调，不再走任何分支。
                    limitWarned = true;
                    std::cerr << "[Warning] Simulator::step() 达到单步回调上限 ("
                              << kMaxCallbacksPerStep << ")，触发限流保护！" << std::endl;
                }
            }
        }
    };

    // 组合逻辑两步迭代传播。
    // 为什么是 2 轮？单级门电路 SW -> AND -> LED 的链路：
    //   第 1 轮：网络传播把 SW 电平送到 AND 输入；门计算更新 AND 输出引脚；
    //   第 2 轮：网络传播把 AND 输出送到 LED；门计算无新增变化。
    // 即一轮"传播 + 门计算"无法把门输出再传到下游引脚，需要 2 轮才能走完单级链路。
    // （更深层链路需改拓扑排序，本周范围内 2 轮够用。）
    for (int iter = 0; iter < 2; ++iter) {
        // 1. 沿网络传播：扫描每个 Net，找到第一个非 Undefined 电平作为网络电平，
        //    广播给同网络下所有引脚（适配单引脚 SWITCH(输出) 和 LED(输入)，通过 Net 自动串联）。
        for (const auto& net : m_schematic.nets) {
            SignalLevel netLevel = SignalLevel::Undefined;
            for (const auto& pin : net.pins) {
                SignalLevel lvl = query(pin.componentId, pin.pinIndex);
                if (lvl != SignalLevel::Undefined) {
                    netLevel = lvl;
                    break;  // 取第一个驱动源即可，本周不支持总线冲突检测
                }
            }
            if (netLevel != SignalLevel::Undefined) {
                for (const auto& pin : net.pins) {
                    updatePinState(pin.componentId, pin.pinIndex, netLevel);
                }
            }
        }

        // 2. 逻辑门计算 (支持 AND, OR, NOT)
        //    约定: 双输入门输入脚为 0 和 1，输出脚为 2；单输入非门输入脚为 0，输出脚为 1
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
            // SWITCH 和 LED 分别作为纯源和纯汇，无需在此处计算，由 Net 传播自动维系
        }
    }
}

// 查询某引脚当前电平。
// 返回语义：未设置过的引脚返回 Undefined（而非 Low），这样 step() 里的网络传播
// 能区分"尚未驱动"与"已被拉低"——这是 setInput 用 find 而非 operator[] 的配套设计。
SignalLevel Simulator::query(const std::string& componentId, int pinIndex) const {
    auto it = m_pinStates.find(makeKey(componentId, pinIndex));
    if (it != m_pinStates.end()) {
        return it->second;
    }
    return SignalLevel::Undefined;
}

} // namespace editor
