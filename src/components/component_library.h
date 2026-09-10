#pragma once
// 元件库(负责人 C,对应任务 3)
// UI 的元件面板靠这里列出可选元件;放置元件时 SchematicModel 也从这里取引脚模板。

#include <string>
#include <vector>

#include "contract/data_model.h"

namespace editor {

class ComponentLibrary {
public:
    /// 库里所有元件类型,如 {"AND", "OR", "NOT", "SWITCH", "LED"}
    std::vector<std::string> types() const;

    /// 类型的中文显示名,如 "AND" -> "与门"
    std::string displayName(const std::string& type) const;

    /// 某类型的引脚模板(名称/方向/相对位置),UI 画符号和连线要用
    std::vector<PinDescriptor> pinTemplate(const std::string& type) const;
};

} // namespace editor
