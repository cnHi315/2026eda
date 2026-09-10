#pragma once
// 原理图数据管理(负责人 A,对应任务 4/5 的数据侧)
// 契约见 src/contract/data_model.h;接口汇总见 docs/interfaces.md

#include <string>

#include "contract/data_model.h"

namespace editor {

class SchematicModel {
public:
    /// 只读访问全部数据(UI 渲染用)
    const Schematic& data() const;

    /// 放置一个指定类型的新元件,返回新元件 id;失败返回空串。
    /// type 从 ComponentLibrary::types() 里取。
    std::string addElement(const std::string& type, Point pos);

    /// 移除元件(并断开其相连导线),成功返回 true。
    bool removeElement(const std::string& id);

    /// 移动元件到新坐标,成功返回 true。
    bool moveElement(const std::string& id, Point pos);

    /// 连接两个引脚,返回网络 id;失败返回空串。
    /// net 生成规则:from 或 to 已在某网络中则并入该网络,否则新建网络。
    /// 拓扑约束:不允许两个输出引脚直连;不允许重复连线。
    std::string addWire(PinRef from, PinRef to);

    /// 断开指定网络(删除该网络的全部导线),成功返回 true。
    bool removeWire(const std::string& netId);

    /// 按 id 查找元件;不存在返回 nullptr。
    const Component* findComponent(const std::string& id) const;

private:
    Schematic schematic_;
};

} // namespace editor
