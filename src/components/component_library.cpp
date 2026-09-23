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
        return {
            {"A", PinDirection::Input,  {-50, 0}},
            {"Y", PinDirection::Output, { 50, 0}},
        };
    }

    if (type == "LED") {
        return {
            {"A", PinDirection::Input,  {-50, 0}},
            {"K", PinDirection::Output, { 50, 0}},
        };
    }

    return {};  // 未知类型返回空，调用方需自行处理
}

} // namespace editor