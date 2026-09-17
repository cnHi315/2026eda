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

bool SchematicModel::removeWire(const std::string& netId) {
    (void)netId;
    return false;  // TODO(A)
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

} // namespace editor
