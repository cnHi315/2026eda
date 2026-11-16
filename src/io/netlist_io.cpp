#include "io/netlist_io.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "io/json.hpp"

// TODO(A): 每完成一项对照 docs/test-plan.md 的 T-04/T-05 核对。
//
// 两种文件格式:
//   save / load   —— 本项目自己的 JSON(version 1),结构见 docs/data-model.md
//   exportNetlist —— KiCad 的 s-expression 网表(.net),Pcbnew 可以直接导入

namespace editor {

namespace {

// ---------------------------------------------------------------- JSON 工具

nlohmann::json pointToJson(Point p) {
    return nlohmann::json{{"x", p.x}, {"y", p.y}};
}

Point pointFromJson(const nlohmann::json& j) {
    Point p;
    p.x = j.value("x", 0);
    p.y = j.value("y", 0);
    return p;
}

nlohmann::json pinRefToJson(const PinRef& r) {
    return nlohmann::json{{"componentId", r.componentId}, {"pinIndex", r.pinIndex}};
}

PinRef pinRefFromJson(const nlohmann::json& j) {
    PinRef r;
    r.componentId = j.at("componentId").get<std::string>();  // 必需字段,缺了就抛
    r.pinIndex    = j.value("pinIndex", 0);
    return r;
}

// ------------------------------------------------------------------ 文件写入

/// 先写临时文件再改名。写了一半失败也不会把用户原来的文件毁掉。
///
/// 两个跨平台坑,别改回去:
///   1. 判断流有没有打开要用 is_open(),**不能**用 if (!out)。MinGW 的
///      std::ifstream/ofstream 打开失败时不置 failbit,`!out` 恒为 false。
///   2. Windows 的 std::rename 在目标已存在时会失败,POSIX 却会直接覆盖。
///      先 remove 掉目标,两边行为就一致了。
bool writeFile(const std::string& path, const std::string& content) {
    // 路径一律按 UTF-8 解释(u8path):Windows 上窄字符路径走 ANSI 代码页,
    // 中文文件名会打不开或写到别处。
    const std::filesystem::path dst = std::filesystem::u8path(path);
    const std::filesystem::path tmp = std::filesystem::u8path(path + ".tmp");

    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out.is_open()) return false;   // 目录不存在 / 没权限
        out << content;
        if (!out.good()) return false;      // 写一半失败(磁盘满等)
    }
    std::error_code ec;
    std::filesystem::remove(dst, ec);       // Windows 的 rename 不会覆盖已存在的目标
    std::filesystem::rename(tmp, dst, ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return false;
    }
    return true;
}

} // namespace

// ============================================================== 保存 / 打开

bool NetlistIO::save(const Schematic& schematic, const std::string& path) {
    nlohmann::json j;
    j["version"] = 1;

    j["components"] = nlohmann::json::array();
    for (const Component& c : schematic.components) {
        nlohmann::json jc;
        jc["id"]       = c.id;
        jc["type"]     = c.type;
        jc["name"]     = c.name;
        jc["pos"]      = pointToJson(c.pos);
        jc["rotation"] = c.rotation;

        jc["pins"] = nlohmann::json::array();
        for (const PinDescriptor& p : c.pins) {
            jc["pins"].push_back(nlohmann::json{
                {"name", p.name},
                {"direction", p.direction == PinDirection::Output ? "output" : "input"},
                {"relPos", pointToJson(p.relPos)},
            });
        }
        j["components"].push_back(jc);
    }

    j["wires"] = nlohmann::json::array();
    for (const Wire& w : schematic.wires) {
        j["wires"].push_back(nlohmann::json{
            {"id", w.id},
            {"from", pinRefToJson(w.from)},
            {"to", pinRefToJson(w.to)},
        });
    }

    // net 是从 wires 推导出来的,存进去只是为了文件可读;load 那边不靠它
    j["nets"] = nlohmann::json::array();
    for (const Net& n : schematic.nets) {
        nlohmann::json jn;
        jn["id"]   = n.id;
        jn["name"] = n.name;
        jn["pins"] = nlohmann::json::array();
        for (const PinRef& p : n.pins) jn["pins"].push_back(pinRefToJson(p));
        j["nets"].push_back(jn);
    }

    return writeFile(path, j.dump(2) + "\n");
}

bool NetlistIO::load(Schematic& schematic, const std::string& path) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in.is_open()) return false;  // 文件打不开(别写 if (!in),见 writeFile 的注释)

    nlohmann::json j;
    try {
        in >> j;
    } catch (const nlohmann::json::exception&) {
        return false;  // 不是合法 JSON
    }

    // 先读到临时对象。中途任何一步失败,schematic 都保持原样。
    Schematic tmp;
    try {
        for (const nlohmann::json& jc : j.at("components")) {
            Component c;
            c.id       = jc.at("id").get<std::string>();
            c.type     = jc.at("type").get<std::string>();
            c.name     = jc.value("name", c.id);
            c.pos      = pointFromJson(jc.value("pos", nlohmann::json::object()));
            c.rotation = jc.value("rotation", 0);

            for (const nlohmann::json& jp : jc.value("pins", nlohmann::json::array())) {
                PinDescriptor p;
                p.name      = jp.value("name", "");
                p.direction = (jp.value("direction", "input") == "output")
                                  ? PinDirection::Output
                                  : PinDirection::Input;
                p.relPos    = pointFromJson(jp.value("relPos", nlohmann::json::object()));
                c.pins.push_back(p);
            }
            tmp.components.push_back(c);
        }

        for (const nlohmann::json& jw : j.value("wires", nlohmann::json::array())) {
            Wire w;
            w.id   = jw.at("id").get<std::string>();
            w.from = pinRefFromJson(jw.at("from"));
            w.to   = pinRefFromJson(jw.at("to"));
            tmp.wires.push_back(w);
        }

        for (const nlohmann::json& jn : j.value("nets", nlohmann::json::array())) {
            Net n;
            n.id   = jn.at("id").get<std::string>();
            n.name = jn.value("name", "");
            for (const nlohmann::json& jp : jn.value("pins", nlohmann::json::array()))
                n.pins.push_back(pinRefFromJson(jp));
            tmp.nets.push_back(n);
        }
    } catch (const nlohmann::json::exception&) {
        return false;  // 字段缺失或类型不对
    }

    schematic = tmp;  // 走到这里才覆盖
    return true;
}

// ================================================================== 网表导出

bool NetlistIO::exportNetlist(const Schematic& schematic, const std::string& path) {
    // gEDA/Pcb 网表:每行「网络名 + 若干 RefDes-引脚号」。引脚号 = pinIndex + 1。
    std::ostringstream out;
    for (const Net& n : schematic.nets) {
        out << (n.name.empty() ? n.id : n.name) << '\t';
        for (const PinRef& p : n.pins) {
            out << p.componentId << '-' << (p.pinIndex + 1) << ' ';
        }
        out << '\n';
    }
    return writeFile(path, out.str());
}

bool NetlistIO::exportPcbLayout(const Schematic& schematic, const std::string& path) {
    // gEDA pcb 的布局文件。逻辑坐标 1 像素 = 10 mil;每个元件自带一套引脚,
    // 引脚号与 exportNetlist 完全一致(1..N),所以 .net 能被这个 .pcb 直接吃下。
    std::ostringstream out;
    out << "# release: pcb 1.99x\n"
           "FileVersion[20070407]\n"
           "PCB[\"\" 600000 500000]\n"
           "Grid[10000.000000 0 0 0]\n"
           "Cursor[0 0 0.000000]\n"
           "PolyArea[200000000.000000]\n"
           "Thermal[0.500000]\n"
           "DRC[1000 1000 1000 1000 1500 1000]\n"
           "Flags(\"nameonpcb,uniquename,clearnew,snappin\")\n"
           "Groups(\"1,c:2:3:4:5:6,s:7:8\")\n"
           "Styles[\"Signal,1000,3600,2000,1000:Power,2500,6000,3500,1000:"
           "Fat,4000,6000,3500,1000:Skinny,600,2402,1181,600\"]\n\n";

    const int kMilPerPixel = 10;
    for (const Component& c : schematic.components) {
        const int n = static_cast<int>(c.pins.size());
        const int width = (n > 0 ? n : 1) * 100;

        out << "Element(0x00 \"" << c.type << "\" \"" << c.id << "\" \"" << c.id
            << "\" " << c.pos.x * kMilPerPixel << ' ' << c.pos.y * kMilPerPixel
            << " 0 100 0x00)\n(\n";
        for (int i = 0; i < n; ++i) {
            // 1 号脚是方焊盘(0x101),其余圆焊盘(0x01)—— 与参考文件一致
            out << "\tPin(" << (50 + i * 100) << " 50 60 28 \"" << (i + 1) << "\" "
                << (i == 0 ? "0x101" : "0x01") << ")\n";
        }
        out << "\tElementLine(0 0 " << width << " 0 10)\n";
        out << "\tElementLine(" << width << " 0 " << width << " 200 10)\n";
        out << "\tElementLine(" << width << " 200 0 200 10)\n";
        out << "\tElementLine(0 200 0 0 10)\n";
        out << ")\n";
    }

    static const char* kLayers[] = {"top",     "ground",  "signal2", "signal3", "power",
                                    "bottom",  "outline", "spare",   "silk",    "silk"};
    for (int i = 0; i < 10; ++i) {
        out << "Layer(" << (i + 1) << " \"" << kLayers[i] << "\")\n(\n)\n";
    }
    return writeFile(path, out.str());
}

} // namespace editor
