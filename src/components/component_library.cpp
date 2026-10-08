#include "component_library.h"

namespace editor {

std::vector<std::string> ComponentLibrary::types() const {
    return {"AND", "OR", "NOT", "SWITCH", "LED"};
}

std::string ComponentLibrary::displayName(const std::string& type) const {
    if (type == "AND")    return "与门";
    if (type == "OR")     return "或门";
    if (type == "NOT")    return "非门";
    if (type == "SWITCH") return "开关";
    if (type == "LED")    return "LED";
    return type;  // 未知类型原样返回，避免 UI 空白
}

std::vector<PinDescriptor> ComponentLibrary::pinTemplate(const std::string& type) const {
    if (type == "AND") {
        return {
            {"A", PinDirection::Input,  {-50, -10}},
            {"B", PinDirection::Input,  {-50,  10}},
            {"Y", PinDirection::Output, { 50,   0}},
        };
    }

    if (type == "OR") {
        return {
            {"A", PinDirection::Input,  {-50, -10}},
            {"B", PinDirection::Input,  {-50,  10}},
            {"Y", PinDirection::Output, { 50,   0}},
        };
    }

    if (type == "NOT") {
        return {
            {"A", PinDirection::Input,  {-50, 0}},
            {"Y", PinDirection::Output, { 50, 0}},
        };
    }

    if (type == "SWITCH") {
        // 第 4 周定稿:开关是"源",收敛成 1 个引脚(输出)。
        // 这样 setInput(id, 0, …) 直接就是输出脚,电平能进网络(原先两脚时 0 号是输入脚,拨开关是死的)。
        return {
            {"Y", PinDirection::Output, { 50, 0}},
        };
    }

    if (type == "LED") {
        // 第 4 周定稿:LED 是"汇",收敛成 1 个引脚(输入)。
        return {
            {"A", PinDirection::Input,  {-50, 0}},
        };
    }

    return {};  // 未知类型返回空，调用方需自行处理
}

} // namespace editor
