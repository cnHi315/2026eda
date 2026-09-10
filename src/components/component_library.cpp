#include "components/component_library.h"

// TODO(C): 先做 AND / OR / NOT / SWITCH / LED 五个元件。
// 引脚相对坐标(relPos)要和 B 商量好符号大小,画布才能对齐。

namespace editor {

std::vector<std::string> ComponentLibrary::types() const {
    return {};  // TODO(C)
}

std::string ComponentLibrary::displayName(const std::string& type) const {
    (void)type;
    return "";  // TODO(C)
}

std::vector<PinDescriptor> ComponentLibrary::pinTemplate(const std::string& type) const {
    (void)type;
    return {};  // TODO(C)
}

} // namespace editor
