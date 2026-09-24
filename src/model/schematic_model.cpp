#include "model/schematic_model.h"

#include "components/component_library.h"

// TODO(A): 逐方法实现,每完成一项对照 docs/test-plan.md 的 T-02/T-03 核对。

namespace editor {

namespace {

// 元件 id 的前缀(见 docs/data-model.md 的 "U1"、"SW1"、"LED1")。
// 没列在这里的新类型会退化成用类型名本身当前缀,所以 C 加元件不必等这边改。
std::string typePrefix(const std::string& type) {
    if (type == "AND" || type == "OR" || type == "NOT") return "U";
    if (type == "SWITCH") return "SW";
    if (type == "LED")    return "LED";
    return type;
}

} // namespace

const Schematic& SchematicModel::data() const {
    return schematic_;
}

std::string SchematicModel::addElement(const std::string& type, Point pos) {
    ComponentLibrary lib;
    std::vector<PinDescriptor> pins = lib.pinTemplate(type);
    if (pins.empty()) return "";   // 库里没这个类型

    Component comp;
    comp.type = type;
    comp.pos  = pos;
    comp.pins = pins;
    comp.id   = newComponentId(type);
    comp.name = comp.id;           // 显示名默认和 id 一样,用户改名时再覆盖

    schematic_.components.push_back(comp);
    // 新元件还没接线,nets 不受影响,不用重算
    return comp.id;
}

bool SchematicModel::removeElement(const std::string& id) {
    // 1. 先找到并删掉元件本身
    int idx = -1;
    for (int i = 0; i < (int)schematic_.components.size(); ++i) {
        if (schematic_.components[i].id == id) { idx = i; break; }
    }
    if (idx == -1) return false;
    schematic_.components.erase(schematic_.components.begin() + idx);

    // 2. 把挂在这个元件上的导线一并删掉(任一端是它就算)
    std::vector<Wire> keep;
    for (const Wire& wire : schematic_.wires) {
        if (wire.from.componentId != id && wire.to.componentId != id)
            keep.push_back(wire);
    }
    schematic_.wires.swap(keep);

    // 3. 连接关系变了,nets 要重算
    rebuildNets();
    return true;
}

bool SchematicModel::moveElement(const std::string& id, Point pos) {
    // 只改坐标。Wire 里存的是 PinRef 不是坐标,所以连接关系一点没变,nets 不用动。
    for (Component& comp : schematic_.components) {
        if (comp.id == id) {
            comp.pos = pos;
            return true;
        }
    }
    return false;
}

std::string SchematicModel::addWire(PinRef from, PinRef to) {
    const Component* comp1=findComponent(from.componentId);
    const Component* comp2=findComponent(to.componentId);
    Net* net1=findNetOf(from);
    Net* net2=findNetOf(to);

    if (comp1 && comp2){
        // 连线合法判断
        if (from.componentId == to.componentId 
            && from.pinIndex == to.pinIndex)  // 自连
                return "";
        if(from.pinIndex < 0 || from.pinIndex >= (int)comp1->pins.size()) return "";
        if(to.pinIndex < 0 || to.pinIndex >= (int)comp2->pins.size()) return "";
        if(comp1->pins[from.pinIndex].direction == PinDirection::Output
            && comp2->pins[to.pinIndex].direction == PinDirection::Output)
                return "";
        for(Wire &wire : schematic_.wires){
            PinRef from_ =wire.from;
            PinRef to_ =wire.to;
            if(from.componentId == from_.componentId
                && from.pinIndex == from_.pinIndex
                && to.componentId == to_.componentId
                && to.pinIndex == to_.pinIndex)
                    return "";
            if(to.componentId == from_.componentId
                && to.pinIndex == from_.pinIndex
                && from.componentId == to_.componentId
                && from.pinIndex == to_.pinIndex)
                    return "";
        }
        // 连线
        Wire wire;
        wire.from = from;
        wire.to = to;
        wire.id = newWireId();
        schematic_.wires.push_back(wire);
        // net 构造
        if (net1 && net2){ // 都有net
            if (net1 == net2) return net1->id;
            for(PinRef &pin : net2->pins){
                net1->pins.push_back(pin);
            }
            std::string mergeId = net1->id; // 避免net1在最后的情况
            if(net2 != &schematic_.nets.back()) // swap-and-pop
                std::swap(schematic_.nets.back(), *net2);
            schematic_.nets.pop_back();
            return mergeId;
        }
        else if (!net1 && !net2){ // 都无net
            Net net;
            net.id = newNetId();
            net.pins.push_back(from);
            net.pins.push_back(to);
            schematic_.nets.push_back(net);
            return net.id;
        }
        else { // 仅一个有net
            if (net1){
                net1->pins.push_back(to);
                return net1->id;
            } 
            else {
                net2->pins.push_back(from);
                return net2->id;
            }
            
        }
    }
    return "";
}

bool SchematicModel::removeWire(const std::string& wireId) {
    for (int i = 0; i < (int)schematic_.wires.size(); ++i) {
        if (schematic_.wires[i].id == wireId) {
            schematic_.wires.erase(schematic_.wires.begin() + i);
            // 少了这条边,原来的网络可能被拆开 —— 重算一遍
            rebuildNets();
            return true;
        }
    }
    return false;
}

const Component* SchematicModel::findComponent(const std::string& id) const {
    // (void)id;
    // return nullptr;  // TODO(A)
    for (const Component &comp : schematic_.components){
        if (comp.id == id) return &comp;
    }
    return nullptr;
}

Net* SchematicModel::findNetOf(const PinRef& pin){
    for (Net &net : schematic_.nets){
        for (const PinRef& pin_ : net.pins){
            if (pin.componentId == pin_.componentId
                && pin.pinIndex == pin_.pinIndex) return &net;
        }
    }
    return nullptr;
}

std::string SchematicModel::newComponentId(const std::string& type){
    const std::string prefix = typePrefix(type);
    for(int i=1;;++i){
        std::string id = prefix + std::to_string(i);
        if (findComponent(id) == nullptr) return id;
    }
}

std::string SchematicModel::newNetId(){
    for(int i=1;;++i){
        std::string id="net" + std::to_string(i);
        bool used = false;
        for (Net& net : schematic_.nets){
            if(net.id == id){
                used=true; break;
            }
        }
        if(!used) return id;
    }
}

std::string SchematicModel::newWireId(){
    for(int i=1;;++i){
        std::string id="wire" + std::to_string(i);
        bool used = false;
        for (Wire& wire : schematic_.wires){
            if(wire.id == id){
                used=true; break;
            }
        }
        if(!used) return id;
    }
}

// 从 wires_ 重新推导 nets_:把每条线当成一条边,求连通分量,每个分量合成一个网络。
// 删线 / 删元件之后调用,保证"两个引脚相连 ⟺ 在同一个网络里"。
void SchematicModel::rebuildNets(){
    schematic_.nets.clear();
    if (schematic_.wires.empty()) return;

    // 1. 每条线把它的两个端点并进同一组。groups 的每个元素是一组互连的引脚。
    std::vector<std::vector<PinRef>> groups;
    for (const Wire& wire : schematic_.wires){
        // 看两个端点各自落在哪一组(没出现过就是 -1)
        int gi = -1, gj = -1;
        for (int k = 0; k < (int)groups.size(); ++k){
            for (const PinRef& p : groups[k]){
                if (p.componentId == wire.from.componentId && p.pinIndex == wire.from.pinIndex) gi = k;
                if (p.componentId == wire.to.componentId   && p.pinIndex == wire.to.pinIndex)   gj = k;
            }
        }
        if (gi == -1 && gj == -1){          // 两个端点都是新的 -> 开一组
            groups.push_back({wire.from, wire.to});
        } else if (gi == -1){               // from 是新的 -> 塞进 to 那组
            groups[gj].push_back(wire.from);
        } else if (gj == -1){               // to 是新的 -> 塞进 from 那组
            groups[gi].push_back(wire.to);
        } else if (gi != gj){               // 分属两组 -> 合到一起
            for (const PinRef& p : groups[gj]) groups[gi].push_back(p);
            groups.erase(groups.begin() + gj);
        }
        // gi == gj:这条线两端本来就通(并联),不用动
    }

    // 2. 每组一个网络。只剩一个引脚的组不成网络。
    for (const std::vector<PinRef>& group : groups){
        if (group.size() < 2) continue;
        Net net;
        net.pins = group;
        net.id = newNetId();   // nets_ 是空的,所以会依次拿到 net1、net2 …
        schematic_.nets.push_back(net);
    }
}

} // namespace editor
