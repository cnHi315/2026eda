// 仿真器单元自测的独立控制台入口。
// 运行: .\build\Debug\sim_test.exe
#include "simulation/simulator.h"

#include <windows.h>

int main() {
    // 让 Windows 终端按 UTF-8 解码 stdout,配合 /utf-8 编译选项解决中文乱码。
    SetConsoleOutputCP(CP_UTF8);

    editor::testAndGateTruthTable();
    return 0;
}
