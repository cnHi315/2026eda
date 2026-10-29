// ComponentLibrary 自测的独立控制台入口 —— 不依赖 UI,也不依赖 wxWidgets。
//
// 编译运行:
//   cmake --build build
//   ./build/components_test     (Windows 上是 build\components_test.exe)

#include <iostream>
#include <string>

#include "components/component_library.h"

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

    /// C-T-01: types() 返回 5 个,内容与顺序
    void testTypes() {
        std::cout << "\n== C-T-01. types() ==\n";
        ComponentLibrary lib;
        auto t = lib.types();
        CHECK(t.size() == 5);
        CHECK(t.size() == 5 && t[0] == "AND");
        CHECK(t.size() == 5 && t[1] == "OR");
        CHECK(t.size() == 5 && t[2] == "NOT");
        CHECK(t.size() == 5 && t[3] == "SWITCH");
        CHECK(t.size() == 5 && t[4] == "LED");
    }

    /// C-T-02: displayName 五个中文名
    void testDisplayName() {
        std::cout << "\n== C-T-02. displayName() ==\n";
        ComponentLibrary lib;
        CHECK(lib.displayName("AND") == "与门");
        CHECK(lib.displayName("OR") == "或门");
        CHECK(lib.displayName("NOT") == "非门");
        CHECK(lib.displayName("SWITCH") == "开关");
        CHECK(lib.displayName("LED") == "LED");
        CHECK(lib.displayName("UNKNOWN") == "UNKNOWN");
    }

    /// C-T-03: AND 三引脚
    void testAnd() {
        std::cout << "\n== C-T-03. pinTemplate(\"AND\") ==\n";
        ComponentLibrary lib;
        auto p = lib.pinTemplate("AND");
        CHECK(p.size() == 3);
        CHECK(p.size() == 3 && p[0].name == "A");
        CHECK(p.size() == 3 && p[0].direction == PinDirection::Input);
        CHECK(p.size() == 3 && p[0].relPos.x == -50 && p[0].relPos.y == -10);
        CHECK(p.size() == 3 && p[1].name == "B");
        CHECK(p.size() == 3 && p[1].direction == PinDirection::Input);
        CHECK(p.size() == 3 && p[1].relPos.x == -50 && p[1].relPos.y == 10);
        CHECK(p.size() == 3 && p[2].name == "Y");
        CHECK(p.size() == 3 && p[2].direction == PinDirection::Output);
        CHECK(p.size() == 3 && p[2].relPos.x == 50 && p[2].relPos.y == 0);
    }

    /// C-T-04: OR 三引脚,与 AND 对称
    void testOr() {
        std::cout << "\n== C-T-04. pinTemplate(\"OR\") ==\n";
        ComponentLibrary lib;
        auto p = lib.pinTemplate("OR");
        CHECK(p.size() == 3);
        CHECK(p.size() == 3 && p[0].name == "A" && p[0].relPos.x == -50 && p[0].relPos.y == -10);
        CHECK(p.size() == 3 && p[1].name == "B" && p[1].relPos.x == -50 && p[1].relPos.y == 10);
        CHECK(p.size() == 3 && p[2].name == "Y" && p[2].relPos.x == 50 && p[2].relPos.y == 0);
        CHECK(p.size() == 3 && p[2].direction == PinDirection::Output);
    }

    /// C-T-05: NOT 两引脚
    void testNot() {
        std::cout << "\n== C-T-05. pinTemplate(\"NOT\") ==\n";
        ComponentLibrary lib;
        auto p = lib.pinTemplate("NOT");
        CHECK(p.size() == 2);
        CHECK(p.size() == 2 && p[0].name == "A");
        CHECK(p.size() == 2 && p[0].direction == PinDirection::Input);
        CHECK(p.size() == 2 && p[0].relPos.x == -50 && p[0].relPos.y == 0);
        CHECK(p.size() == 2 && p[1].name == "Y");
        CHECK(p.size() == 2 && p[1].direction == PinDirection::Output);
        CHECK(p.size() == 2 && p[1].relPos.x == 50 && p[1].relPos.y == 0);
    }

    /// C-T-09: SWITCH 单脚(源)
    void testSwitch() {
        std::cout << "\n== C-T-09. pinTemplate(\"SWITCH\") ==\n";
        ComponentLibrary lib;
        auto p = lib.pinTemplate("SWITCH");
        CHECK(p.size() == 1);
        CHECK(p.size() == 1 && p[0].name == "Y");
        CHECK(p.size() == 1 && p[0].direction == PinDirection::Output);
        CHECK(p.size() == 1 && p[0].relPos.x == 50 && p[0].relPos.y == 0);
    }

    /// C-T-10: LED 单脚(汇)
    void testLed() {
        std::cout << "\n== C-T-10. pinTemplate(\"LED\") ==\n";
        ComponentLibrary lib;
        auto p = lib.pinTemplate("LED");
        CHECK(p.size() == 1);
        CHECK(p.size() == 1 && p[0].name == "A");
        CHECK(p.size() == 1 && p[0].direction == PinDirection::Input);
        CHECK(p.size() == 1 && p[0].relPos.x == -50 && p[0].relPos.y == 0);
    }

    /// C-T-08 / C-T-11: 未知类型返回空(画布据此拒绝放置)
    void testUnknown() {
        std::cout << "\n== C-T-08 / C-T-11. pinTemplate(未知类型) ==\n";
        ComponentLibrary lib;
        CHECK(lib.pinTemplate("UNKNOWN").empty());
        CHECK(lib.pinTemplate("").empty());
        CHECK(lib.pinTemplate("and").empty());
    }

} // namespace

int main() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
#endif

    testTypes();
    testDisplayName();
    testAnd();
    testOr();
    testNot();
    testSwitch();
    testLed();
    testUnknown();

    std::cout << "\n===== 合计 " << g_checks << " 项, 失败 " << g_failures << " =====\n";
    return g_failures == 0 ? 0 : 1;
}