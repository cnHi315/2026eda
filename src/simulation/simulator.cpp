#include "simulation/simulator.h"
#include <iostream>

namespace editor {

// ---------------------------------------------------------------------------
// 辅助工具
// ---------------------------------------------------------------------------

std::string Simulator::makeKey(const std::string& compId, int pinIndex) {
    return compId + "#" + std::to_string(pinIndex);
}

bool Simulator::isSequentialType(const std::string& type) {
    // 精确匹配已知时序元件类型
    static const char* kExactMatch[] = {
        "DFF", "JK", "JKFF", "SR", "SRFF", "CLOCK", "LATCH"
    };
    for (const char* kw : kExactMatch) {
        if (type == kw) {
            return true;
        }
    }
    // 再放长子串匹配:DFF_RESET / JK_ASYNC 等扩展名
    static const char* kSubstring[] = { "DFF", "CLOCK", "LATCH" };
    for (const char* kw : kSubstring) {
        if (type.find(kw) != std::string::npos) {
            return true;
        }
    }
    // 注意:故意不单独匹配 "FF" —— 会误命中 BUFFER / OFFSET 等无关类型
    return false;
}

void Simulator::setSignalCallback(Callback cb) {
    m_callback = cb;
}

// ---------------------------------------------------------------------------
// 环路检测
// ---------------------------------------------------------------------------

bool Simulator::detectCombinationalLoop(const Schematic& schematic) {
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

    // 检查某个引脚所属元件是否为时序元件
    auto isSequential = [&schematic, &index](const PinRef& p) -> bool {
        auto it = index.find(p.componentId);
        if (it == index.end()) {
            return false;
        }
        return Simulator::isSequentialType(schematic.components[it->second].type);
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
                // —— 关键:跳过连到时序元件输入引脚的边 ——
                // 时序反馈(Q→D)是合法时序行为,不属于组合环路。
                if (isSequential(to)) {
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

// ---------------------------------------------------------------------------
// 生命周期
// ---------------------------------------------------------------------------

void Simulator::load(const Schematic& schematic) {
    m_schematic = schematic;
    m_pinStates.clear();
    m_ffStates.clear();
    m_hasLoop = detectCombinationalLoop(m_schematic);
    if (m_hasLoop) {
        std::cerr << "[Warning] 检测到环路:电路存在组合反馈,仿真结果可能不稳定" << std::endl;
    }
}

bool Simulator::hasCombinationalLoop() const {
    return m_hasLoop;
}

// ---------------------------------------------------------------------------
// 输入设置
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// 仿真推进
// ---------------------------------------------------------------------------

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

    // ================================================================
    // 阶段 1:组合逻辑两轮传播
    //   先让 SWITCH 等输入驱动的网络电平稳定,包括 DFF 的 D 输入引脚
    // ================================================================
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

    // ================================================================
    // 阶段 2:时序元件(DFF)仿真
    //   组合逻辑先跑过了,DFF.D 引脚已有正确电平,
    //   此时捕获 D 并更新 Q 输出(相当于一个隐式时钟边沿)
    // ================================================================
    for (const auto& comp : m_schematic.components) {
        if (!isSequentialType(comp.type)) {
            continue;
        }
        if (comp.pins.size() < 2) {
            continue;
        }
        int dPin = -1;
        int qPin = -1;
        for (int i = 0; i < static_cast<int>(comp.pins.size()); ++i) {
            const PinDescriptor& p = comp.pins[i];
            if (dPin == -1 && p.direction == PinDirection::Input) {
                dPin = i;
            }
            if (qPin == -1 && p.direction == PinDirection::Output) {
                qPin = i;
            }
        }
        if (dPin == -1 || qPin == -1) {
            continue;
        }
        SignalLevel dLvl = query(comp.id, dPin);
        m_ffStates[comp.id] = dLvl;
        updatePinState(comp.id, qPin, dLvl);
    }

    // ================================================================
    // 阶段 3:最后一轮网络传播
    //   DFF.Q 刚更新的值需要扩散到它驱动的组合路径
    // ================================================================
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
}

// ---------------------------------------------------------------------------
// 查询
// ---------------------------------------------------------------------------

SignalLevel Simulator::query(const std::string& componentId, int pinIndex) const {
    auto it = m_pinStates.find(makeKey(componentId, pinIndex));
    if (it != m_pinStates.end()) {
        return it->second;
    }
    return SignalLevel::Undefined;
}

} // namespace editor

// ================================================================
// 自测块:hasCombinationalLoop 行为验证 + DFF 仿真基本功能
//
// 设计说明:
//   - 用 C++ 静态初始化让自测在 sim_test 等 console 程序里自动执行,
//     CircuitEditor(WIN32 GUI) 里 stdout 被抑制,自测输出无害。
//   - 全部手写 Schematic 数据,不依赖 ComponentLibrary/SchematicModel,
//     避免 addWire 的拓扑校验妨碍环路测试。
// ================================================================

#include <cassert>

#if defined(_WIN32)
#include <windows.h>        // SetConsoleOutputCP
#endif

using namespace editor;

static int g_checks = 0;
static int g_failures = 0;

#define ST_CHECK(cond)                                                         \
    do {                                                                     \
        ++g_checks;                                                          \
        if (cond) {                                                          \
            std::cout << "  [PASS] " #cond "\n";                             \
        } else {                                                             \
            std::cout << "  [FAIL] " #cond "   (第 " << __LINE__ << " 行)\n"; \
            ++g_failures;                                                    \
        }                                                                    \
    } while (0)

namespace {

// ---- hasCombinationalLoop:正常电路(无环路)----
void testLoopFreeCircuit() {
    std::cout << "\n== hasCombinationalLoop:SWITCH→AND→LED(无环路) ==\n";
    Schematic sch;
    sch.components = {
        {"SW1", "SWITCH", "SW1", {0, 0}, 0, {{{"Y", PinDirection::Output, {0, 0}}}}},
        {"SW2", "SWITCH", "SW2", {0, 50}, 0, {{{"Y", PinDirection::Output, {0, 0}}}}},
        {"U1",  "AND",    "U1",  {100, 25}, 0, {
            {"A", PinDirection::Input, {-50, -10}},
            {"B", PinDirection::Input, {-50,  10}},
            {"Y", PinDirection::Output, { 50,   0}}
        }},
        {"LED1", "LED",   "LED1", {200, 25}, 0, {{{"A", PinDirection::Input, {0, 0}}}}}
    };
    sch.nets = {
        {"net1", "", {{"SW1", 0}, {"U1", 0}}},
        {"net2", "", {{"SW2", 0}, {"U1", 1}}},
        {"net3", "", {{"U1", 2},  {"LED1", 0}}}
    };

    Simulator sim;
    sim.load(sch);
    ST_CHECK(!sim.hasCombinationalLoop());
}

// ---- hasCombinationalLoop:单 NOT 自环 ----
// NOT 门 Y(Out) 直接接回 A(In),最直观的组合环路
void testNotSelfLoop() {
    std::cout << "\n== hasCombinationalLoop:单 NOT 自环(有环路) ==\n";
    Schematic sch;
    sch.components = {
        {"U1", "NOT", "U1", {0, 0}, 0, {
            {"A", PinDirection::Input,  {-50, 0}},
            {"Y", PinDirection::Output, { 50, 0}}
        }}
    };
    sch.nets = {
        {"net1", "", {{"U1", 1}, {"U1", 0}}}
    };

    Simulator sim;
    sim.load(sch);
    ST_CHECK(sim.hasCombinationalLoop());
}

// ---- hasCombinationalLoop:两个 NOT 门互相反馈 ----
// U1.Y → U2.A, U2.Y → U1.A(两个网络串联形成组合环路)
void testTwoNotFeedbackLoop() {
    std::cout << "\n== hasCombinationalLoop:NOT 交叉耦合(有环路) ==\n";
    Schematic sch;
    sch.components = {
        {"U1", "NOT", "U1", {0, 0}, 0, {
            {"A", PinDirection::Input,  {-50, 0}},
            {"Y", PinDirection::Output, { 50, 0}}
        }},
        {"U2", "NOT", "U2", {200, 0}, 0, {
            {"A", PinDirection::Input,  {-50, 0}},
            {"Y", PinDirection::Output, { 50, 0}}
        }}
    };
    sch.nets = {
        {"net1", "", {{"U1", 1}, {"U2", 0}}},
        {"net2", "", {{"U2", 1}, {"U1", 0}}}
    };

    Simulator sim;
    sim.load(sch);
    ST_CHECK(sim.hasCombinationalLoop());
}

// ---- 时序反馈(DFF):不应该被误判为组合环路 ----
// SWITCH → AND → DFF → AND → SWITCH... 反馈路径穿过 DFF,DFF 边跳过
void testSequentialNotCombinationalLoop() {
    std::cout << "\n== hasCombinationalLoop:经过 DFF 的反馈不构成组合环路 ==\n";
    Schematic sch;
    sch.components = {
        {"SW1", "SWITCH", "SW1", {0, 0}, 0, {{{"Y", PinDirection::Output, {0, 0}}}}},
        {"U1",  "AND",    "U1",  {100, 0}, 0, {
            {"A", PinDirection::Input,  {-50, -10}},
            {"B", PinDirection::Input,  {-50,  10}},
            {"Y", PinDirection::Output, { 50,   0}}
        }},
        {"FF1", "DFF",    "FF1", {200, 0}, 0, {
            {"D", PinDirection::Input,  {-50, 0}},
            {"Q", PinDirection::Output, { 50, 0}}
        }}
    };
    // SW1.Y → U1.A; FF1.Q → U1.B; U1.Y → FF1.D
    sch.nets = {
        {"net1", "", {{"SW1", 0}, {"U1", 0}}},
        {"net2", "", {{"FF1", 1}, {"U1", 1}}},   // FF1.Q → U1.B(组合边保留)
        {"net3", "", {{"U1", 2},  {"FF1", 0}}}    // U1.Y → FF1.D(时序边跳过)
    };

    Simulator sim;
    sim.load(sch);
    ST_CHECK(!sim.hasCombinationalLoop());
}

// ---- DFF 仿真:独立驱动下 Q 在 step() 后正确更新 ----
// SWITCH → NOT → DFF.D,无反馈路径,step 后 Q 应该 == NOT(SWITCH)
void testDFFBasicBehavior() {
    std::cout << "\n== DFF 仿真:SWITCH→NOT→DFF.D,step 后 Q == NOT(SWITCH) ==\n";
    Schematic sch;
    sch.components = {
        {"SW1", "SWITCH", "SW1", {0, 0}, 0, {{{"Y", PinDirection::Output, {0, 0}}}}},
        {"U1",  "NOT",    "U1",  {100, 0}, 0, {
            {"A", PinDirection::Input,  {-50, 0}},
            {"Y", PinDirection::Output, { 50, 0}}
        }},
        {"FF1", "DFF",    "FF1", {200, 0}, 0, {
            {"D", PinDirection::Input,  {-50, 0}},
            {"Q", PinDirection::Output, { 50, 0}}
        }},
        {"LED1", "LED",   "LED1", {300, 0}, 0, {{{"A", PinDirection::Input, {0, 0}}}}}
    };
    // SW1.Y → U1.A; U1.Y → FF1.D; FF1.Q → LED1.A(观测 Q 输出)
    sch.nets = {
        {"net1", "", {{"SW1", 0}, {"U1", 0}}},
        {"net2", "", {{"U1", 1},  {"FF1", 0}}},
        {"net3", "", {{"FF1", 1}, {"LED1", 0}}}
    };

    Simulator sim;
    sim.load(sch);

    // case 1: SWITCH=High → NOT=Low → 初始 Q=Undefined,step 后 Q=Low
    sim.setInput("SW1", 0, SignalLevel::High);
    sim.step();
    ST_CHECK(sim.query("FF1", 1) == SignalLevel::Low);
    ST_CHECK(sim.query("LED1", 0) == SignalLevel::Low);

    // case 2: SWITCH=Low → NOT=High → step 后 Q=High
    sim.setInput("SW1", 0, SignalLevel::Low);
    sim.step();
    ST_CHECK(sim.query("FF1", 1) == SignalLevel::High);
    ST_CHECK(sim.query("LED1", 0) == SignalLevel::High);
}

} // namespace

void runSimulatorSelfTests() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "\n========== Simulator 自测 ==========\n";

    testLoopFreeCircuit();
    testNotSelfLoop();
    testTwoNotFeedbackLoop();
    testSequentialNotCombinationalLoop();
    testDFFBasicBehavior();

    std::cout << "\n---- " << (g_checks - g_failures) << "/" << g_checks
              << " 通过 ----\n";
}

// C++ 静态初始化阶段自动跑自测 —— main_test.cpp 里的 main() 不能改,
// 用这个技巧让自测块在任何链接了 simulator.cpp 的可执行文件里自动执行。
// 在 CircuitEditor(WIN32 GUI)里 stdout 未连接,自测输出无害,只是浪费几毫秒。
// Release 构建(NDEBUG 已定义)里不跑,避免 GUI 程序静默吞掉潜在错误。
#ifndef NDEBUG
static int g_runSelfTests = []() {
    runSimulatorSelfTests();
    return 0;
}();
#endif // !NDEBUG
