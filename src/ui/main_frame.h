#pragma once
// 主窗口(负责人 B):菜单栏 + 工具栏 + 状态栏 + 三栏布局。
// 布局:左 = 元件库(ComponentPalette),中 = 画布(CanvasPanel),右 = 属性表(PropertyPanel)。
// 阶段四:窗口持有 SchematicModel / ComponentLibrary / NetlistIO / Simulator,
//   把菜单(文件 / 编辑)、元件树、画布、属性表与仿真串起来。

#include <wx/frame.h>
#include <wx/string.h>

#include "components/component_library.h"
#include "io/netlist_io.h"
#include "model/schematic_model.h"
#include "simulation/simulator.h"

class ComponentPalette;
class CanvasPanel;
class PropertyPanel;

class UiMainFrame : public wxFrame {
public:
    UiMainFrame();

private:
    void BuildMenuBar();
    void BuildToolBar();
    void BuildStatusBar();
    void BuildLayout();
    void BindCommands();

    void OnNew(wxCommandEvent& evt);
    void OnOpen(wxCommandEvent& evt);
    void OnSave(wxCommandEvent& evt);
    void OnSaveAs(wxCommandEvent& evt);
    void OnExportNetlist(wxCommandEvent& evt);
    void OnDeleteSelection(wxCommandEvent& evt);

    bool SaveDocument(const wxString& path);
    std::string Utf8Path(const wxString& path) const;
    void RefreshCounts();
    void UpdateTitle();

    ComponentPalette* m_palette = nullptr;
    CanvasPanel* m_canvas = nullptr;
    PropertyPanel* m_props = nullptr;

    editor::SchematicModel m_model;
    editor::ComponentLibrary m_lib;
    editor::NetlistIO m_io;
    editor::Simulator m_sim;
    wxString m_path;   ///< 当前文件路径;空 = 未命名文档
};
