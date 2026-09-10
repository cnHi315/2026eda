#pragma once
// 文件功能(负责人 A,对应任务 5):保存/打开原理图、导出标准网表。
// JSON 读写开工时引入 nlohmann/json 单头文件,放到本目录即可。

#include <string>

#include "contract/data_model.h"

namespace editor {

class NetlistIO {
public:
    /// 保存为 JSON,失败返回 false。
    bool save(const Schematic& schematic, const std::string& path);

    /// 从 JSON 读入(先清空 schematic 再重建,id 保持文件中的值),失败返回 false。
    bool load(Schematic& schematic, const std::string& path);

    /// 导出文本网表(格式自定,先导出 KiCad 的网表研究它的格式),失败返回 false。
    bool exportNetlist(const Schematic& schematic, const std::string& path);
};

} // namespace editor
