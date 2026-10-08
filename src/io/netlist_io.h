#pragma once
// 文件功能(负责人 A,对应任务 5):保存/打开原理图、导出标准网表。
// JSON 读写开工时引入 nlohmann/json 单头文件,放到本目录即可。

#include <string>

#include "contract/data_model.h"

namespace editor {

// exportNetlist 输出的格式:KiCad 的 s-expression 网表(.net),Pcbnew 可直接导入。
// 格式出处 https://dev-docs.kicad.org/en/file-formats/sexpr-intro/index.html
//
//   (export (version D)
//     (components
//       (comp (ref "U1") (value "AND")
//         (libsource (lib "2026eda") (part "AND"))
//         (sheetpath (names "/") (tstamps "/"))
//         (tstamp <UUID>)))
//     (nets
//       (net (code 1) (name "net1")
//         (node (ref "SW1") (pin "Y"))
//         (node (ref "U1")  (pin "A")))))
//
// 要点:
//   - 引脚写**名字**(PinDescriptor::name),不是下标
//   - tstamp 由元件 id 用 FNV-1a 推出来,所以同一份原理图每次导出结果完全一样,可 diff
//   - 没有 footprint 字段 —— 项目没建模 PCB 封装。Pcbnew 导入后会提示"未指定封装",
//     在那里再分配即可,属于正常流程
//   - net 名取 Net::name,空则退回 Net::id;code 从 1 连续编号

class NetlistIO {
public:
    /// 保存为 JSON,失败返回 false。
    bool save(const Schematic& schematic, const std::string& path);

    /// 从 JSON 读入(先清空 schematic 再重建,id 保持文件中的值),失败返回 false。
    bool load(Schematic& schematic, const std::string& path);

    /// 导出 KiCad 的 s-expression 网表(.net),Pcbnew 可以直接导入;失败返回 false。
    /// 注:本项目没有建模 PCB 封装(Component 里没有 footprint 字段),导入后
    ///     会提示"未指定封装",在 Pcbnew 里再分配即可 —— 这是正常流程。
    bool exportNetlist(const Schematic& schematic, const std::string& path);
};

} // namespace editor
