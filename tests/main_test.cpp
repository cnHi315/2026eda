// 仿真器自测的独立控制台入口 —— 不依赖 UI,也不依赖 wxWidgets。
//
// 编译运行:
//   cmake --build build
//   ./build/sim_test          (Windows 上是 build\sim_test.exe)
//
// 注意:这个目标要在 Windows 和 Linux 上都能编。
//      用到平台专有的东西,一律用 #ifdef _WIN32 包起来,并保证另一条分支也能编译。

#include <cassert>
#include <iostream>

#include "simulation/simulator.h"

#if defined(_WIN32)
#include <windows.h>        // 只为下面的 SetConsoleOutputCP
#endif

using editor::Component;
using editor::Net;
using editor::PinDirection;
using editor::Schematic;
using editor::SignalLevel;
using editor::Simulator;

namespace {

/// 脱离 UI 验证与门真值表:手写一份假的原理图数据喂给 Simulator。
void testAndGateTruthTable() {
    std::cout << "\n>>> 与门真值表自测 <<<" << std::endl;

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

} // namespace

int main() {
#if defined(_WIN32)
    // 让 Windows 终端按 UTF-8 解码 stdout,否则中文输出是乱码。
    // Linux 终端本来就是 UTF-8,这一步不需要。
    SetConsoleOutputCP(CP_UTF8);
#endif

    testAndGateTruthTable();
    return 0;
}
