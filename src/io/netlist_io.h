#pragma once
// 文件功能(负责人 A,对应任务 5):保存/打开原理图、导出标准网表。
// JSON 读写开工时引入 nlohmann/json 单头文件,放到本目录即可。

#include <string>

#include "contract/data_model.h"

namespace editor {

// 导出给 PCB 用的两个文件(任务 5 的交付物,对标 gEDA:gschem + pcb):
//
//   exportNetlist    → .net(gEDA/Pcb 网表),每行一个网络:
//                        GND<TAB>U3-7 U1-7 U2-7
//                      即「网络名 + 若干 RefDes-引脚号」
//
//   exportPcbLayout  → .pcb(gEDA pcb 的初始布局),每个元件一个 Element(...),
//                      其 Pin 编号与网表里的引脚号一致
//
// 两边的 RefDes 都直接用元件 id(U1 / SW1 / LED1),引脚号 = pinIndex + 1,
// 所以两个文件天然对得上,不需要额外的映射表。
//
// 一个约定:项目没有建模真实封装,导出的 .pcb 里每个元件自带一套引脚,
// 封装名取元件类型(AND / SWITCH …)。pcb 加载后即可飞线、布线。

class NetlistIO {
public:
    /// 保存为 JSON,失败返回 false。
    bool save(const Schematic& schematic, const std::string& path);

    /// 从 JSON 读入(先清空 schematic 再重建,id 保持文件中的值),失败返回 false。
    bool load(Schematic& schematic, const std::string& path);

    /// 导出 gEDA/Pcb 网表(.net):每行「网络名 + RefDes-引脚号 …」;失败返回 false。
    bool exportNetlist(const Schematic& schematic, const std::string& path);

    /// 导出 gEDA pcb 的初始布局文件(.pcb):每个元件一个 Element,引脚号与网表一致。
    bool exportPcbLayout(const Schematic& schematic, const std::string& path);
};

} // namespace editor
