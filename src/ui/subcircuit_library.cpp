#include "ui/subcircuit_library.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include <algorithm>
#include <set>
#include "components/component_library.h"
#include "io/netlist_io.h"
#include "model/schematic_model.h"

namespace {
bool Fail(wxString* error, const char* message) {
    if (error) *error = wxString::FromUTF8(message);
    return false;
}
}

SubcircuitLibrary::SubcircuitLibrary(const wxString& directory) : m_directory(directory) {}

bool SubcircuitLibrary::Validate(const editor::Schematic& fragment, wxString* error) {
    if (fragment.components.empty()) return Fail(error, "子电路没有元件");
    editor::ComponentLibrary library;
    std::set<std::string> ids, wires;
    for (const auto& c : fragment.components) {
        if (c.id.empty() || !ids.insert(c.id).second) return Fail(error, "子电路的元件编号重复或为空");
        const auto pins = library.pinTemplate(c.type);
        if (pins.empty() || pins.size() != c.pins.size() || c.rotation != 0)
            return Fail(error, "子电路包含当前不支持的元件格式或旋转");
        for (size_t i = 0; i < pins.size(); ++i) {
            if (pins[i].name != c.pins[i].name || pins[i].direction != c.pins[i].direction ||
                pins[i].relPos.x != c.pins[i].relPos.x || pins[i].relPos.y != c.pins[i].relPos.y)
                return Fail(error, "子电路的引脚格式与当前元件库不兼容");
        }
    }
    for (const auto& w : fragment.wires) {
        if (w.id.empty() || !wires.insert(w.id).second || !ids.count(w.from.componentId) || !ids.count(w.to.componentId))
            return Fail(error, "子电路包含无效或跨边界的连线");
    }
    // Validate against the actual model, without touching the open document.
    editor::SchematicModel probe;
    const auto created = probe.instantiate(fragment, {0, 0});
    if (created.size() != fragment.components.size() || probe.data().wires.size() != fragment.wires.size())
        return Fail(error, "子电路包含无效引脚或不允许的连线");
    return true;
}

size_t SubcircuitLibrary::Reload() {
    m_items.clear();
    if (!wxDirExists(m_directory)) return 0;
    wxDir directory(m_directory);
    if (!directory.IsOpened()) return 1;
    wxArrayString files;
    wxString filename;
    bool more = directory.GetFirst(&filename, wxEmptyString, wxDIR_FILES);
    while (more) {
        if (filename.Lower().EndsWith(".json")) files.Add(filename);
        more = directory.GetNext(&filename);
    }
    files.Sort();
    size_t skipped = 0;
    editor::NetlistIO io;
    for (const auto& file : files) {
        editor::Schematic schematic;
        wxFileName path(m_directory, file);
        if (!io.load(schematic, path.GetFullPath().ToStdString(wxConvUTF8)) || !Validate(schematic, nullptr)) {
            ++skipped;
            continue;
        }
        m_items.push_back({path.GetName(), std::move(schematic)});
    }
    return skipped;
}

bool SubcircuitLibrary::Save(wxString name, const editor::Schematic& fragment, bool overwrite, wxString* error) {
    name.Trim().Trim(false);
    if (name.empty() || name == "." || name == ".." || name.EndsWith(".") ||
        name.find_first_of("\\/:*?\"<>|\r\n\t") != wxString::npos)
        return Fail(error, "名称不能为空，也不能包含路径或文件名特殊字符");
    wxString device = name.BeforeFirst('.').Upper();
    if (device == "CON" || device == "PRN" || device == "AUX" || device == "NUL" ||
        (device.length() == 4 && (device.StartsWith("COM") || device.StartsWith("LPT")) &&
         device[3] >= '1' && device[3] <= '9'))
        return Fail(error, "请换一个可用的子电路名称");
    if (!Validate(fragment, error)) return false;
    if (!wxDirExists(m_directory) && !wxFileName::Mkdir(m_directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL))
        return Fail(error, "无法创建子电路目录");
    const wxFileName path(m_directory, name + ".json");
    if (path.FileExists() && !overwrite) return Fail(error, "已有同名子电路，请换名或确认覆盖");
    auto normalized = fragment;
    int minX = normalized.components.front().pos.x, minY = normalized.components.front().pos.y;
    for (const auto& c : normalized.components) { minX = std::min(minX, c.pos.x); minY = std::min(minY, c.pos.y); }
    for (auto& c : normalized.components) { c.pos.x -= minX; c.pos.y -= minY; }
    editor::SchematicModel model;
    model.loadFrom(normalized); // Derive nets solely from the retained internal wires.
    editor::NetlistIO io;
    if (!io.save(model.data(), path.GetFullPath().ToStdString(wxConvUTF8))) return Fail(error, "保存子电路失败，请检查目录是否可写");
    Reload();
    return true;
}
