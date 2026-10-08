#include "io/netlist_io.h"

#include <cstdio>
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

// ------------------------------------------------------------- 网表导出工具

/// 由元件 id 生成一个稳定的 UUID 字符串(KiCad 的 comp 节点要这个字段)。
/// 同一份原理图每次导出结果都一样,方便 git diff。
std::string stableUuid(const std::string& seed) {
    auto fnv = [](const std::string& s) {
        unsigned long long h = 1469598103934665603ULL;
        for (unsigned char ch : s) {
            h ^= ch;
            h *= 1099511628211ULL;
        }
        return h;
    };
    const unsigned long long a = fnv(seed);
    const unsigned long long b = fnv(seed + "|2026eda");
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%08llx-%04llx-%04llx-%04llx-%012llx",
                  (a >> 32) & 0xFFFFFFFFULL,
                  (a >> 16) & 0xFFFFULL,
                  a & 0xFFFFULL,
                  (b >> 48) & 0xFFFFULL,
                  b & 0xFFFFFFFFFFFFULL);
    return std::string(buf);
}

const Component* findIn(const Schematic& s, const std::string& id) {
    for (const Component& c : s.components)
        if (c.id == id) return &c;
    return nullptr;
}

/// 网表里的引脚写成名字("A" / "Y")而不是下标 —— KiCad 按名字认引脚。
std::string pinLabelOf(const Schematic& s, const PinRef& r) {
    const Component* c = findIn(s, r.componentId);
    if (c && r.pinIndex >= 0 && r.pinIndex < (int)c->pins.size())
        return c->pins[r.pinIndex].name;
    return std::to_string(r.pinIndex);  // 兜底:认不出来就用下标
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
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out.is_open()) return false;   // 目录不存在 / 没权限
        out << content;
        if (!out.good()) return false;      // 写一半失败(磁盘满等)
    }
    std::remove(path.c_str());
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        std::remove(tmp.c_str());
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
    std::ifstream in(path, std::ios::binary);
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
    std::ostringstream out;
    out << "(export (version D)\n";

    out << "  (design\n";
    out << "    (source \"" << path << "\")\n";
    out << "    (tool \"2026eda CircuitEditor\")\n";
    out << "  )\n";

    out << "  (components\n";
    for (const Component& c : schematic.components) {
        out << "    (comp (ref \"" << c.id << "\")\n";
        out << "      (value \"" << c.type << "\")\n";
        out << "      (libsource (lib \"2026eda\") (part \"" << c.type << "\"))\n";
        out << "      (sheetpath (names \"/\") (tstamps \"/\"))\n";
        out << "      (tstamp " << stableUuid(c.id) << ")\n";
        out << "    )\n";
    }
    out << "  )\n";

    out << "  (nets\n";
    int code = 1;
    for (const Net& n : schematic.nets) {
        out << "    (net (code " << code++ << ") (name \""
            << (n.name.empty() ? n.id : n.name) << "\")\n";
        for (const PinRef& p : n.pins) {
            out << "      (node (ref \"" << p.componentId << "\") (pin \""
                << pinLabelOf(schematic, p) << "\"))\n";
        }
        out << "    )\n";
    }
    out << "  )\n";

    out << ")\n";
    return writeFile(path, out.str());
}

} // namespace editor
