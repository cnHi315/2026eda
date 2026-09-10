#pragma once
// 电路仿真(负责人 D,对应任务 1、任务 6)
// 交互参考 Logisim:拨开关 -> 信号沿导线传播 -> 输出变化,UI 收到回调后刷新颜色。

#include <functional>
#include <string>

#include "contract/data_model.h"

namespace editor {

class Simulator {
public:
    /// 电平变化回调:UI 用 lambda 注册,刷新导线颜色 / LED 亮灭。
    using Callback = std::function<void(const PinRef& pin, SignalLevel level)>;

    void setSignalCallback(Callback cb);

    /// 从原理图建立仿真模型(读取元件类型与连接关系)。
    void load(const Schematic& schematic);

    /// 设置输入元件(开关等)引脚电平。
    void setInput(const std::string& componentId, int pinIndex, SignalLevel level);

    /// 推进一次稳定传播:输入变化后沿拓扑依次更新输出。
    void step();

    /// 查询某引脚当前电平。
    SignalLevel query(const std::string& componentId, int pinIndex) const;
};

} // namespace editor
