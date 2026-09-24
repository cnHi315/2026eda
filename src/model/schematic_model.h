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
    /// net 生成规则(四种情况):都不在则新建;一个在则并入;同在则不改动;分别在不同网络则合并。
    /// 拓扑约束:不允许两个输出引脚直连;同一条线不能重复(两个端点相同,正反都算)。
    std::string addWire(PinRef from, PinRef to);

    /// 删掉一条导线,成功返回 true;找不到该 id 返回 false。
    /// 参数是 Wire::id,不是网络 id。删完之后 nets 会整体重算(见 docs/data-model.md)。
    bool removeWire(const std::string& wireId);

    /// 按 id 查找元件;不存在返回 nullptr。
    const Component* findComponent(const std::string& id) const;

private:
    // 查询引脚所属网络，没找到返回nullptr
    Net* findNetOf(const PinRef& pir);

    // 从 wires_ 重新推导 nets_(删线 / 删元件之后调用)
    void rebuildNets();

    std::string newComponentId(const std::string& type);
    std::string newNetId();
    std::string newWireId();

    Schematic schematic_;
};

} // namespace editor
