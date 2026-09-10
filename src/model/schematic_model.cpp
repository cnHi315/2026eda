#include "model/schematic_model.h"

// TODO(A): 逐方法实现,每完成一项对照 docs/test-plan.md 的 T-02/T-03 核对。

namespace editor {

const Schematic& SchematicModel::data() const {
    return schematic_;
}

std::string SchematicModel::addElement(const std::string& type, Point pos) {
    (void)type;
    (void)pos;
    return "";  // TODO(A): 生成 "U1" 这类全局唯一 id,填好 pins(从 ComponentLibrary 取模板)
}

bool SchematicModel::removeElement(const std::string& id) {
    (void)id;
    return false;  // TODO(A)
}

bool SchematicModel::moveElement(const std::string& id, Point pos) {
    (void)id;
    (void)pos;
    return false;  // TODO(A)
}

std::string SchematicModel::addWire(PinRef from, PinRef to) {
    (void)from;
    (void)to;
    return "";  // TODO(A): 并入已有 net 或新建 net,见头文件注释
}

bool SchematicModel::removeWire(const std::string& netId) {
    (void)netId;
    return false;  // TODO(A)
}

const Component* SchematicModel::findComponent(const std::string& id) const {
    (void)id;
    return nullptr;  // TODO(A)
}

} // namespace editor
