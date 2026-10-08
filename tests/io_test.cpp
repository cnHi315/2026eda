// 文件功能自测(保存 / 打开 / 导出网表)—— 不依赖 UI,也不依赖 wxWidgets。
//
// 编译运行:
//   cmake --build build
//   ./build/io_test          (Windows 上是 build\io_test.exe)
//
// 注意:要在仓库根目录下跑,临时文件写在 build/ 里(该目录已 gitignore)。

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "io/netlist_io.h"
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

const char* kJson = "build/io_test_demo.json";
const char* kNet  = "build/io_test_demo.net";

// 注意:这里必须用 is_open()。MinGW 的 ifstream 打开失败时不置 failbit,
// `in.good()` 会返回 true,判断文件在不在就永远是"在"。
bool fileExists(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    return in.is_open();
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

void spit(const std::string& path, const std::string& content) {
    std::ofstream out(path, std::ios::binary);
    out << content;
}

/// 造一份小电路:两个开关 + 一个与门 + 一个 LED,三条线(T-04 用的就是它)
SchematicModel makeDemo() {
    SchematicModel m;
    m.addElement("SWITCH", {100, 100});   // SW1
    m.addElement("SWITCH", {100, 300});   // SW2
    m.addElement("AND",    {300, 200});   // U1
    m.addElement("LED",    {500, 200});   // LED1
    m.addWire({"SW1", 1}, {"U1", 0});     // wire1
    m.addWire({"SW2", 1}, {"U1", 1});     // wire2
    m.addWire({"U1", 2},  {"LED1", 0});   // wire3
    return m;
}

/// 保存 → 打开,数据要一模一样(T-04)
void testRoundTrip() {
    std::cout << "\n== 1. 保存 → 打开,数据一模一样 ==\n";
    const SchematicModel m = makeDemo();
    NetlistIO io;

    CHECK(io.save(m.data(), kJson));
    CHECK(fileExists(kJson));

    Schematic back;
    CHECK(io.load(back, kJson));

    CHECK(back.components.size() == m.data().components.size());
    CHECK(back.wires.size() == m.data().wires.size());
    CHECK(back.nets.size() == m.data().nets.size());

    // id 必须原样回来 —— B 的选中态、导线端点都按它索引
    bool idsSame = true;
    for (size_t i = 0; i < back.components.size(); ++i) {
        if (back.components[i].id != m.data().components[i].id) idsSame = false;
    }
    CHECK(idsSame);

    CHECK(back.components.size() == 4);
    CHECK(back.components[0].id == "SW1");
    CHECK(back.components[2].id == "U1");
    CHECK(back.components[2].pos.x == 300);
    CHECK(back.components[2].pos.y == 200);
    CHECK(back.components[2].pins.size() == 3);

    // 引脚的名字 / 方向 / 相对位置都要回来
    CHECK(back.components[2].pins[2].name == "Y");
    CHECK(back.components[2].pins[2].direction == PinDirection::Output);
    CHECK(back.components[2].pins[0].name == "A");
    CHECK(back.components[2].pins[0].direction == PinDirection::Input);
    CHECK(back.components[2].pins[0].relPos.x == -50);

    // 导线
    CHECK(back.wires[0].id == "wire1");
    CHECK(back.wires[0].from.componentId == "SW1");
    CHECK(back.wires[0].from.pinIndex == 1);
    CHECK(back.wires[0].to.componentId == "U1");
    CHECK(back.wires[0].to.pinIndex == 0);

    // 网络
    CHECK(back.nets[0].pins.size() == 2);
}

/// 文件打不开 / 内容不对时,传进来的 schematic 不能被动
void testBadInput() {
    std::cout << "\n== 2. 打不开 / 坏文件时,原数据不能被破坏 ==\n";
    NetlistIO io;
    Schematic s;
    s.components.push_back(Component{"KEEP", "AND", "KEEP", {7, 7}, 0, {}});

    CHECK(io.load(s, "build/definitely_not_here.json") == false);
    CHECK(s.components.size() == 1);
    CHECK(s.components[0].id == "KEEP");

    spit("build/io_test_bad.json", "{ 这不是合法的 JSON");
    CHECK(io.load(s, "build/io_test_bad.json") == false);
    CHECK(s.components.size() == 1);
    CHECK(s.components[0].id == "KEEP");

    // 合法 JSON,但字段缺了(没有 components)
    spit("build/io_test_nofield.json", R"({"version": 1})");
    CHECK(io.load(s, "build/io_test_nofield.json") == false);
    CHECK(s.components.size() == 1);
}

/// 文件里的 nets 可能是过时的 —— loadFrom 一律按 wires 重算
void testLoadFromRebuildsNets() {
    std::cout << "\n== 3. loadFrom:文件里的 nets 不作数 ==\n";
    const char* stale =
        R"({
  "version": 1,
  "components": [
    {"id":"SW1","type":"SWITCH","name":"SW1","pos":{"x":0,"y":0},"rotation":0,
     "pins":[{"name":"A","direction":"input","relPos":{"x":-50,"y":0}},
             {"name":"Y","direction":"output","relPos":{"x":50,"y":0}}]},
    {"id":"LED1","type":"LED","name":"LED1","pos":{"x":200,"y":0},"rotation":0,
     "pins":[{"name":"A","direction":"input","relPos":{"x":-50,"y":0}},
             {"name":"K","direction":"output","relPos":{"x":50,"y":0}}]}
  ],
  "wires": [
    {"id":"wire1","from":{"componentId":"SW1","pinIndex":1},
                 "to":{"componentId":"LED1","pinIndex":0}}
  ],
  "nets": []
})";
    spit("build/io_test_stale.json", stale);

    NetlistIO io;
    Schematic s;
    CHECK(io.load(s, "build/io_test_stale.json"));
    CHECK(s.components.size() == 2);
    CHECK(s.nets.empty());              // 文件里 nets 就是空的

    SchematicModel m;
    m.loadFrom(s);
    CHECK(m.data().nets.size() == 1);   // loadFrom 把它算出来了
    CHECK(m.data().nets[0].pins.size() == 2);
    CHECK(m.data().nets[0].id == "net1");
}

/// 导出 KiCad 网表(T-05)
void testExportNetlist() {
    std::cout << "\n== 4. 导出 KiCad 网表 ==\n";
    const SchematicModel m = makeDemo();
    NetlistIO io;

    CHECK(io.exportNetlist(m.data(), kNet));
    CHECK(fileExists(kNet));

    const std::string text = slurp(kNet);

    // s-expression 的基本合法性:括号配平
    int depth = 0;
    bool balanced = true;
    for (char c : text) {
        if (c == '(') ++depth;
        else if (c == ')') { --depth; if (depth < 0) { balanced = false; break; } }
    }
    CHECK(balanced);
    CHECK(depth == 0);

    CHECK(text.find("(export (version D)") != std::string::npos);
    CHECK(text.find("(components") != std::string::npos);
    CHECK(text.find("(nets") != std::string::npos);

    // 四个元件都在
    CHECK(text.find("(ref \"SW1\")") != std::string::npos);
    CHECK(text.find("(ref \"SW2\")") != std::string::npos);
    CHECK(text.find("(ref \"U1\")") != std::string::npos);
    CHECK(text.find("(ref \"LED1\")") != std::string::npos);

    // 网络与引脚:SW1 的 Y 接到 U1 的 A —— 网表里要写引脚名,不是下标
    CHECK(text.find("(name \"net1\")") != std::string::npos);
    CHECK(text.find("(node (ref \"SW1\") (pin \"Y\"))") != std::string::npos);
    CHECK(text.find("(node (ref \"U1\") (pin \"A\"))") != std::string::npos);

    // 导出两次结果必须一样(stableUuid 是确定性的,文件里没有时间戳)
    const std::string first = text;
    CHECK(io.exportNetlist(m.data(), kNet));
    CHECK(slurp(kNet) == first);
}

void testExportEdgeCases() {
    std::cout << "\n== 5. 空原理图 / 写不进去的路径 ==\n";
    NetlistIO io;

    const Schematic empty;
    CHECK(io.exportNetlist(empty, "build/io_test_empty.net"));
    const std::string e = slurp("build/io_test_empty.net");
    CHECK(e.find("(components") != std::string::npos);
    CHECK(e.find("(nets") != std::string::npos);
    CHECK(e.find("(ref ") == std::string::npos);   // 没有元件

    // 目录不存在 -> 失败,而且不留垃圾
    const SchematicModel m = makeDemo();
    CHECK(io.exportNetlist(m.data(), "build/no_such_dir/x.net") == false);
    CHECK(!fileExists("build/no_such_dir/x.net.tmp"));
    CHECK(io.save(m.data(), "build/no_such_dir/x.json") == false);
}

} // namespace

int main() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    testRoundTrip();
    testBadInput();
    testLoadFromRebuildsNets();
    testExportNetlist();
    testExportEdgeCases();

    std::cout << "\n---- " << (g_checks - g_failures) << "/" << g_checks << " 通过 ----\n";
    return g_failures == 0 ? 0 : 1;
}
