// 仿真器单元自测的独立控制台入口(负责人 D)。
// Windows: .\build\Debug\sim_test.exe  |  Linux: ./build/sim_test
#include "simulation/simulator.h"

#if defined(_WIN32)
#include <windows.h>   // SetConsoleOutputCP
#endif

int main() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);   // Windows 终端按 UTF-8 解码 stdout
#endif

    editor::testAndGateTruthTable();
    return 0;
}
