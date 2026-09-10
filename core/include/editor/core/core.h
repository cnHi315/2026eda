#pragma once

namespace editor::core {

// MVT 占位接口：证明 core 层可在无 GUI 环境下编译、链接、测试。
// 后续节点将替换为真正的值类型 / 网表 / 仿真引擎。
int add(int a, int b);

const char* version();

} // namespace editor::core