// A-D 模块联调自测:用 SchematicModel 造真实数据,喂给 Simulator 跑仿真。
//
// 原先是 D 写在 src/simulation/simulator.cpp 末尾、用 #if 0 锁着的一段;注释里自己也
// 写着"真正启用时应按 README 的约定把这段挪到 tests/"。这里正式落地并挂上 CMake 目标。
//
// 编译运行:
//   cmake --build build
//   ./build/integration_test    (Windows 上是 build\integration_test.exe)

#include <iostream>
#include <string>

#include "model/schematic_model.h"
#include "simulation/simulator.h"

#if defined(_WIN32)
#include <windows.h>        // 只为下面的 SetConsoleOutputCP
#endif

using namespace editor;

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                          \
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

/// 两个开关 + 一个与门 + 一个 LED,三条线 —— 需求表 F-05 的那个电路
struct Demo {
    SchematicModel model;
    std::string swA, swB, u, led;
};

Demo makeDemo() {
    Demo d;
    d.swA = d.model.addElement("SWITCH", {0, 0});     // SW1
    d.swB = d.model.addElement("SWITCH", {0, 50});    // SW2
    d.u   = d.model.addElement("AND", {100, 25});     // U1
    d.led = d.model.addElement("LED", {200, 25});     // LED1
    d.model.addWire({d.swA, 0}, {d.u, 0});            // wire1
    d.model.addWire({d.swB, 0}, {d.u, 1});            // wire2
    d.model.addWire({d.u, 2}, {d.led, 0});            // wire3
    return d;
}

/// 模型造的数据能直接喂给仿真器,跑通与门真值表
void testModelFeedsSimulator() {
    std::cout << "\n== 1. SchematicModel 造数据 → Simulator 跑真值表 ==\n";
    Demo d = makeDemo();

    CHECK(d.swA == "SW1");
    CHECK(d.swB == "SW2");
    CHECK(d.u == "U1");
    CHECK(d.led == "LED1");
    CHECK(d.model.data().components.size() == 4);
    CHECK(d.model.data().wires.size() == 3);
    CHECK(d.model.data().nets.size() == 3);
    CHECK(d.model.data().nets[0].pins.size() == 2);

    Simulator sim;
    int callbackCount = 0;
    sim.setSignalCallback([&callbackCount](const PinRef&, SignalLevel) { ++callbackCount; });
    sim.load(d.model.data());
    CHECK(callbackCount == 0);                  // load 本身不该触发回调

    struct Case { SignalLevel a, b, expected; } cases[] = {
        {SignalLevel::Low,  SignalLevel::Low,  SignalLevel::Low},
        {SignalLevel::Low,  SignalLevel::High, SignalLevel::Low},
        {SignalLevel::High, SignalLevel::Low,  SignalLevel::Low},
        {SignalLevel::High, SignalLevel::High, SignalLevel::High},
    };
    for (const Case& tc : cases) {
        sim.setInput(d.swA, 0, tc.a);
        sim.setInput(d.swB, 0, tc.b);
        sim.step();
        CHECK(sim.query(d.led, 0) == tc.expected);
    }
    CHECK(callbackCount > 0);                   // UI 靠它刷新颜色
}

/// 改完数据必须重新 load —— 这条契约写在 docs/interfaces.md
void testReloadAfterEdit() {
    std::cout << "\n== 2. 删掉一根线之后必须重新 load ==\n";
    Demo d = makeDemo();
    Simulator sim;
    sim.load(d.model.data());

    sim.setInput(d.swA, 0, SignalLevel::High);
    sim.setInput(d.swB, 0, SignalLevel::High);
    sim.step();
    CHECK(sim.query(d.led, 0) == SignalLevel::High);

    CHECK(d.model.removeWire("wire2"));                  // 拆掉 SW2 → U1.B
    CHECK(d.model.data().nets.size() == 2);

    // 不重新 load:仿真器手里还是旧副本,结果照旧
    // (这正是 interfaces.md 那条提醒要防的坑 —— 画面已经断了,LED 还亮着)
    sim.setInput(d.swA, 0, SignalLevel::High);
    sim.step();
    CHECK(sim.query(d.led, 0) == SignalLevel::High);

    // 重新 load 之后:U1.B 悬空,与门输出不确定,LED 也就不确定
    sim.load(d.model.data());
    sim.setInput(d.swA, 0, SignalLevel::High);
    sim.step();
    CHECK(sim.query(d.u, 1) == SignalLevel::Undefined);
    CHECK(sim.query(d.led, 0) == SignalLevel::Undefined);
}

/// 真实数据上也跑一遍 OR / NOT
void testOtherGatesOnRealData() {
    std::cout << "\n== 3. OR / NOT 走真实数据 ==\n";
    {
        SchematicModel m;
        const std::string s1 = m.addElement("SWITCH", {0, 0});
        const std::string s2 = m.addElement("SWITCH", {0, 50});
        const std::string g  = m.addElement("OR", {100, 25});
        const std::string l  = m.addElement("LED", {200, 25});
        m.addWire({s1, 0}, {g, 0});
        m.addWire({s2, 0}, {g, 1});
        m.addWire({g, 2}, {l, 0});

        Simulator sim;
        sim.load(m.data());
        sim.setInput(s1, 0, SignalLevel::Low);
        sim.setInput(s2, 0, SignalLevel::High);
        sim.step();
        CHECK(sim.query(l, 0) == SignalLevel::High);     // 0 OR 1 = 1
    }
    {
        SchematicModel m;
        const std::string s = m.addElement("SWITCH", {0, 0});
        const std::string g = m.addElement("NOT", {100, 0});
        const std::string l = m.addElement("LED", {200, 0});
        m.addWire({s, 0}, {g, 0});
        m.addWire({g, 1}, {l, 0});

        Simulator sim;
        sim.load(m.data());
        sim.setInput(s, 0, SignalLevel::High);
        sim.step();
        CHECK(sim.query(l, 0) == SignalLevel::Low);      // NOT 1 = 0
    }
}

} // namespace

int main() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    testModelFeedsSimulator();
    testReloadAfterEdit();
    testOtherGatesOnRealData();

    std::cout << "\n---- " << (g_checks - g_failures) << "/" << g_checks << " 通过 ----\n";
    return g_failures == 0 ? 0 : 1;
}
