// SchematicModel 自测的独立控制台入口 —— 不依赖 UI,也不依赖 wxWidgets。
//
// 编译运行:
//   cmake --build build
//   ./build/model_test          (Windows 上是 build\model_test.exe)
//
// 注意:这个目标要在 Windows 和 Linux 上都能编。
//      用到平台专有的东西,一律用 #ifdef _WIN32 包起来,并保证另一条分支也能编译。

#include <iostream>
#include <string>

#include "model/schematic_model.h"

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

/// 新元件:模板来自 ComponentLibrary(见 T-02)
void testAddElement() {
    std::cout << "\n== 1. addElement ==\n";
    SchematicModel m;

    CHECK(m.addElement("AND", {300, 200}) == "U1");
    CHECK(m.findComponent("U1") != nullptr);
    CHECK(m.findComponent("U1")->pins.size() == 3);      // A / B / Y
    CHECK(m.findComponent("U1")->name == "U1");
    CHECK(m.findComponent("U1")->pos.x == 300);

    CHECK(m.addElement("SWITCH", {100, 100}) == "SW1");
    CHECK(m.addElement("SWITCH", {100, 300}) == "SW2");  // 序号要往后排
    CHECK(m.addElement("LED", {500, 200}) == "LED1");

    CHECK(m.addElement("BOGUS", {0, 0}) == "");          // 库里没有的类型
    CHECK(m.data().components.size() == 4);
    CHECK(m.findComponent("NOPE") == nullptr);
}

/// 连线合法性:自环 / 越界 / 输出直连 / 重复线(见 T-03)
void testAddWireRules() {
    std::cout << "\n== 2. addWire 的四道校验 ==\n";
    SchematicModel m;
    m.addElement("SWITCH", {0, 0});   // SW1:pins[0]=A(入) pins[1]=Y(出)
    m.addElement("AND", {0, 0});      // U1 :pins[0]=A(入) [1]=B(入) [2]=Y(出)
    m.addElement("LED", {0, 0});      // LED1:pins[0]=A(入) [1]=K(出)

    CHECK(m.addWire({"SW1", 1}, {"U1", 0}) == "net1");
    CHECK(m.data().nets.size() == 1);
    CHECK(m.data().nets[0].pins.size() == 2);

    CHECK(m.addWire({"SW1", 1}, {"U1", 0}) == "");       // 重复(正向)
    CHECK(m.addWire({"U1", 0}, {"SW1", 1}) == "");       // 重复(反向)
    CHECK(m.addWire({"U1", 0}, {"U1", 0}) == "");        // 自环
    CHECK(m.addWire({"SW1", 1}, {"LED1", 1}) == "");     // 输出直连输出
    CHECK(m.addWire({"U1", 9}, {"LED1", 0}) == "");      // 引脚下标越界
    CHECK(m.addWire({"NOPE", 0}, {"LED1", 0}) == "");    // 元件不存在
}

/// net 生成的四种情况(见 data-model.md)
void testNetRules() {
    std::cout << "\n== 3. addWire 的四种 net 情况 ==\n";
    SchematicModel m;
    m.addElement("SWITCH", {0, 0});   // SW1
    m.addElement("SWITCH", {0, 0});   // SW2
    m.addElement("OR", {0, 0});       // U1
    m.addElement("LED", {0, 0});      // LED1

    CHECK(m.addWire({"SW1", 1}, {"U1", 0}) == "net1");   // 都不在     -> 新建
    CHECK(m.addWire({"SW1", 1}, {"U1", 1}) == "net1");   // 一个在     -> 并入
    CHECK(m.data().nets.size() == 1);
    CHECK(m.data().nets[0].pins.size() == 3);

    CHECK(m.addWire({"SW2", 1}, {"U1", 1}) == "net1");   // 已连通     -> 不动
    CHECK(m.data().nets[0].pins.size() == 4);

    m.addWire({"U1", 2}, {"LED1", 0});                   // U1.Y 拉出去,另起一个网
    CHECK(m.data().nets.size() == 2);

    // U1.B(入)在 net1、U1.Y(出)在 net2,把它俩接上 == 两个网络合并
    CHECK(m.addWire({"U1", 1}, {"U1", 2}) == "net1");
    CHECK(m.data().nets.size() == 1);
    CHECK(m.data().nets[0].pins.size() == 6);            // 4 + 2
}

/// 本周围绕的核心:删一根线,网络要正确地裂开
void testRemoveWire() {
    std::cout << "\n== 4. removeWire:删中间那根,一个网络裂成两个 ==\n";
    SchematicModel m;
    m.addElement("SWITCH", {0, 0});   // SW1
    m.addElement("AND", {0, 0});      // U1
    m.addElement("LED", {0, 0});      // LED1

    // SW1.Y -- U1.A -- U1.B -- LED1.A,三根线串成一条链
    m.addWire({"SW1", 1}, {"U1", 0});   // wire1
    m.addWire({"U1", 0}, {"U1", 1});    // wire2  <- 待会儿删它
    m.addWire({"U1", 1}, {"LED1", 0});  // wire3
    CHECK(m.data().nets.size() == 1);
    CHECK(m.data().nets[0].pins.size() == 4);

    CHECK(m.removeWire("wire2"));
    CHECK(m.data().wires.size() == 2);
    CHECK(m.data().nets.size() == 2);            // 裂开了
    CHECK(m.data().nets[0].pins.size() == 2);
    CHECK(m.data().nets[1].pins.size() == 2);
    CHECK(m.removeWire("wire999") == false);     // 没有这条线
}

/// 删元件要连它身上的线一起走,剩下的网络也要重算
void testRemoveElement() {
    std::cout << "\n== 5. removeElement:元件和它的线一起走 ==\n";
    SchematicModel m;
    m.addElement("SWITCH", {0, 0});
    m.addElement("LED", {0, 0});
    m.addWire({"SW1", 1}, {"LED1", 0});
    CHECK(m.data().nets.size() == 1);

    CHECK(m.removeElement("LED1"));
    CHECK(m.data().components.size() == 1);
    CHECK(m.data().wires.empty());               // 挂在上面的线一并删掉
    CHECK(m.data().nets.empty());                // 只剩一个引脚,不成网络
    CHECK(m.removeElement("LED1") == false);     // 已经删过了
}

/// 挪元件只改坐标 —— Wire 存的是 PinRef 不是坐标,所以连接关系纹丝不动
void testMoveElement() {
    std::cout << "\n== 6. moveElement:只挪坐标,连接关系一点不动 ==\n";
    SchematicModel m;
    m.addElement("SWITCH", {0, 0});
    m.addElement("LED", {0, 0});
    m.addWire({"SW1", 1}, {"LED1", 0});
    const std::string netIdBefore = m.data().nets[0].id;

    CHECK(m.moveElement("LED1", {800, 600}));
    CHECK(m.findComponent("LED1")->pos.x == 800);
    CHECK(m.findComponent("LED1")->pos.y == 600);
    CHECK(m.data().nets.size() == 1);
    CHECK(m.data().nets[0].id == netIdBefore);   // 网络连 id 都没变
    CHECK(m.data().nets[0].pins.size() == 2);
    CHECK(m.moveElement("NOPE", {0, 0}) == false);
}

/// 并联线:重算时不能把同一个引脚塞进网络两次
void testParallelWire() {
    std::cout << "\n== 7. 并联线:重算后引脚不会重复 ==\n";
    SchematicModel m;
    m.addElement("SWITCH", {0, 0});
    m.addElement("AND", {0, 0});
    m.addElement("LED", {0, 0});
    m.addWire({"SW1", 1}, {"U1", 0});
    m.addWire({"U1", 0}, {"U1", 1});
    m.addWire({"U1", 1}, {"LED1", 0});
    m.addWire({"SW1", 1}, {"LED1", 0});          // 两端本来就通,允许(并联)
    CHECK(m.data().wires.size() == 4);
    CHECK(m.data().nets.size() == 1);
    CHECK(m.data().nets[0].pins.size() == 4);

    CHECK(m.removeWire("wire4"));                // 删掉那根并联线
    CHECK(m.data().nets.size() == 1);            // 网络不变
    CHECK(m.data().nets[0].pins.size() == 4);
}

} // namespace

int main() {
#if defined(_WIN32)
    // 让 Windows 终端按 UTF-8 解码 stdout,否则中文输出是乱码。
    // Linux 终端本来就是 UTF-8,这一步不需要。
    SetConsoleOutputCP(CP_UTF8);
#endif

    testAddElement();
    testAddWireRules();
    testNetRules();
    testRemoveWire();
    testRemoveElement();
    testMoveElement();
    testParallelWire();

    std::cout << "\n---- " << (g_checks - g_failures) << "/" << g_checks << " 通过 ----\n";
    return g_failures == 0 ? 0 : 1;
}
