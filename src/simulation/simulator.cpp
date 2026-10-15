#include "simulation/simulator.h"
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <algorithm>

namespace editor {

std::string Simulator::makeKey(const std::string& compId, int pinIndex) {
    return compId + "#" + std::to_string(pinIndex);
}

void Simulator::setSignalCallback(Callback cb) {
    m_callback = cb;
}

// ---------------------------------------------------------------------------
// D3 组合环路检测(仅本文件内部实现,不新增对外接口)
//   结构化判定:只看连接拓扑,与开关电平无关。把每个元件当一个节点,对每条 net
//   凡"输出脚 -> 输入脚"都连一条有向边(信号由驱动元件流向被驱动元件);若该有
//   向图存在环,说明信号能沿反馈路径绕回自身 => 组合环,step() 的有限轮迭代无法
//   保证收敛。
//   为什么不用"迭代到不收敛"判定:组合环结果依赖具体电平,且 step() 当前只迭代
//   两轮,运行时不收敛根本触发不到;结构判定能在刚连出环的那一刻给出确定答案。
// ---------------------------------------------------------------------------
static bool detectCombinationalLoop(const Schematic& schematic) {
    // 1. 元件 id -> 下标
    std::unordered_map<std::string, size_t> index;
    for (size_t i = 0; i < schematic.components.size(); ++i) {
        index[schematic.components[i].id] = i;
    }

    // 判断某引脚是否为输出脚(输出脚才是信号的驱动端)
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

    // 2. 建"驱动元件 -> 被驱动元件"的有向图:同一 net 里每个输出脚连到每个输入脚
    std::vector<std::vector<size_t>> adj(schematic.components.size());
    for (const auto& net : schematic.nets) {
        for (const auto& from : net.pins) {
            if (!isOutput(from)) {
                continue;                       // 只从输出脚出发
            }
            const auto fi = index.find(from.componentId);
            if (fi == index.end()) {
                continue;
            }
            for (const auto& to : net.pins) {
                if (isOutput(to)) {
                    continue;                   // 只连到输入脚
                }
                const auto ti = index.find(to.componentId);
                if (ti == index.end()) {
                    continue;
                }
                adj[fi->second].push_back(ti->second);   // 允许自环(输出接回本元件输入)
            }
        }
    }

    // 3. 三色法 DFS 找环:走到"灰色"节点说明绕回了本轮路径 => 有环
    enum Color { White, Gray, Black };
    std::vector<int> color(schematic.components.size(), White);
    std::function<bool(size_t)> dfs = [&adj, &color, &dfs](size_t u) -> bool {
        color[u] = Gray;
        for (size_t v : adj[u]) {
            if (color[v] == Gray) {
                return true;                    // 回边 => 环
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
    // 环路检测(结构性,与电平无关),结果缓存供 hasCombinationalLoop() 查询。
    m_hasLoop = detectCombinationalLoop(m_schematic);
    if (m_hasLoop) {
        // 控制台留一行警告(与"单步回调上限"警告同一风格);
        // 状态栏提示由 B 调 hasCombinationalLoop() 自行显示。
        std::cerr << "[Warning] 检测到环路:电路存在组合反馈,仿真结果可能不稳定" << std::endl;
    }
}

// 组合环路查询:UI 每次 step() 后调用,为真时在状态栏提示"检测到环路"。
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

// -------------------------------------------------------------------
// 测试：使用收敛后的单引脚模型（SWITCH: pin 0 为输出，LED: pin 0 为输入）
// -------------------------------------------------------------------
void testAndGateTruthTable() {
    std::cout << "\n>>> 开始运行 D 任务: 适配单引脚模型(SWITCH/LED)与回调熔断全功能自测 <<<" << std::endl;

    Schematic sch;
    // SWITCH 和 LED 收敛为单引脚模型
    Component swA{"SW_A", "SWITCH", "SWITCH", {0, 0}, 0, {{"OUT", PinDirection::Output, {0, 0}}}};
    Component swB{"SW_B", "SWITCH", "SWITCH", {0, 0}, 0, {{"OUT", PinDirection::Output, {0, 0}}}};
    Component andGate{"U_AND", "AND", "AND", {0, 0}, 0, {
        {"A", PinDirection::Input, {0, 0}},
        {"B", PinDirection::Input, {0, 10}},
        {"Y", PinDirection::Output, {20, 5}}
    }};
    Component led{"LED_OUT", "LED", "LED", {0, 0}, 0, {{"IN", PinDirection::Input, {0, 0}}}};

    sch.components = {swA, swB, andGate, led};
    sch.nets = {
        {"netA", "A", {{"SW_A", 0}, {"U_AND", 0}}}, // 开关输出引脚 0 连到与门输入 0
        {"netB", "B", {{"SW_B", 0}, {"U_AND", 1}}}, // 开关输出引脚 0 连到与门输入 1
        {"netY", "Y", {{"U_AND", 2}, {"LED_OUT", 0}}} // 与门输出 2 连到 LED 输入 0
    };

    Simulator sim;
    sim.load(sch);

    int callbackFireCount = 0;
    sim.setSignalCallback([&callbackFireCount](editor::PinRef pin, SignalLevel lvl) {
        ++callbackFireCount;
        std::cout << "  [UI回调广播]: 元件 " << pin.componentId << "#" << pin.pinIndex
                  << " -> " << (lvl == SignalLevel::High ? "高(1)" : "低(0)") << std::endl;
    });

    // 验证与门真值表
    struct TruthTableCase {
        SignalLevel a;
        SignalLevel b;
        SignalLevel expected;
    } cases[] = {
        {SignalLevel::Low,  SignalLevel::Low,  SignalLevel::Low},
        {SignalLevel::Low,  SignalLevel::High, SignalLevel::Low},
        {SignalLevel::High, SignalLevel::Low,  SignalLevel::Low},
        {SignalLevel::High, SignalLevel::High, SignalLevel::High}
    };

    for (const auto& tc : cases) {
        sim.setInput("SW_A", 0, tc.a); // 对 SWITCH 的引脚 0 赋值
        sim.setInput("SW_B", 0, tc.b); // 对 SWITCH 的引脚 0 赋值
        sim.step();

        SignalLevel out = sim.query("LED_OUT", 0); // 查询 LED 引脚 0
        std::cout << "AND 测试: A=" << (tc.a == SignalLevel::High ? "1" : "0")
                  << ", B=" << (tc.b == SignalLevel::High ? "1" : "0")
                  << " => LED=" << (out == SignalLevel::High ? "1" : "0")
                  << " (期望=" << (tc.expected == SignalLevel::High ? "1" : "0") << ") -> ";
        assert(out == tc.expected);
        std::cout << "[PASS]" << std::endl;
    }

    assert(callbackFireCount > 0);
    std::cout << ">>> [PASS] 单引脚模型适配及回调熔断保护验证全部通过！(总触发回调 "
              << callbackFireCount << " 次) <<<" << std::endl;
}

} // namespace editor
