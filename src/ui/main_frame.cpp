#include "ui/main_frame.h"

#include <wx/wx.h>
#include <wx/artprov.h>
#include <wx/filename.h>

#include <string>

#include "ui/canvas_panel.h"
#include "ui/component_palette.h"
#include "ui/property_panel.h"

namespace {

wxString U8(const char* s) { return wxString::FromUTF8(s); }

enum {
    ID_EXPORT = wxID_HIGHEST + 1,
    ID_DELETE_SEL,
};

} // namespace

UiMainFrame::UiMainFrame()
    : wxFrame(nullptr, wxID_ANY, "Circuit Editor",
              wxDefaultPosition, wxSize(1100, 720)) {
    SetMinSize(wxSize(820, 560));

    BuildMenuBar();
    BuildToolBar();
    BuildStatusBar();
    BuildLayout();
    BindCommands();

    Centre();
    UpdateTitle();
    RefreshCounts();
    SetStatusText(U8("就绪 —— 从左侧元件库选元件,点画布放置"));
}

// ---------------------------------------------------------------------------
// 界面骨架
// ---------------------------------------------------------------------------

void UiMainFrame::BuildMenuBar() {
    auto* fileMenu = new wxMenu;
    fileMenu->Append(wxID_NEW, U8("新建(&N)\tCtrl+N"), U8("清空当前原理图"));
    fileMenu->Append(wxID_OPEN, U8("打开(&O)\tCtrl+O"), U8("打开 JSON 原理图"));
    fileMenu->Append(wxID_SAVE, U8("保存(&S)\tCtrl+S"), U8("保存为 JSON"));
    fileMenu->Append(wxID_SAVEAS, U8("另存为(&A)..."), U8("换个文件名保存"));
    fileMenu->AppendSeparator();
    fileMenu->Append(ID_EXPORT, U8("导出网表(&E)\tCtrl+E"), U8("导出 KiCad 网表(.net)"));
    fileMenu->AppendSeparator();
    fileMenu->Append(wxID_EXIT, U8("退出(&Q)\tCtrl+Q"), U8("退出程序"));

    auto* editMenu = new wxMenu;
    editMenu->Append(ID_DELETE_SEL, U8("删除选中(&D)\tDel"),
                     U8("删除选中的元件或导线"));

    auto* bar = new wxMenuBar;
    bar->Append(fileMenu, U8("文件(&F)"));
    bar->Append(editMenu, U8("编辑(&E)"));
    SetMenuBar(bar);
}

void UiMainFrame::BuildToolBar() {
    auto* tb = CreateToolBar();
    tb->AddTool(wxID_NEW, U8("新建"),
                wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR),
                U8("新建空白原理图 (Ctrl+N)"));
    tb->AddTool(wxID_OPEN, U8("打开"),
                wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR),
                U8("打开 (Ctrl+O)"));
    tb->AddTool(wxID_SAVE, U8("保存"),
                wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR),
                U8("保存 (Ctrl+S)"));
    tb->AddSeparator();
    tb->AddTool(ID_EXPORT, U8("导出网表"),
                wxArtProvider::GetBitmap(wxART_EXECUTABLE_FILE, wxART_TOOLBAR),
                U8("导出 KiCad 网表 (Ctrl+E)"));
    tb->Realize();
}

void UiMainFrame::BuildStatusBar() {
    CreateStatusBar(2);
    const int kWidths[] = {-1, 240};
    SetStatusWidths(2, kWidths);
    SetStatusText(U8("就绪"), 0);
    SetStatusText(U8(""), 1);
}

void UiMainFrame::BuildLayout() {
    m_palette = new ComponentPalette(this);
    m_canvas = new CanvasPanel(this);
    m_props = new PropertyPanel(this);

    // —— 阶段四接线:画布 ↔ 模型 / 仿真;元件树 → 放置模式;选中 → 属性表 ——
    m_canvas->Attach(&m_model, &m_sim);
    m_palette->FillFromLibrary(m_lib);

    m_palette->SetTypeCallback([this](const std::string& type) {
        m_canvas->SetPlacementType(type);
    });
    m_canvas->SetStatusCallback([this](const wxString& text) { SetStatusText(text, 0); });
    m_canvas->SetSelectionCallback(
        [this](const editor::Component* c) { m_props->ShowComponent(c); });
    m_canvas->SetChangedCallback([this]() { RefreshCounts(); });

    // 左/右固定宽度,中栏占满剩余空间
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_palette, 0, wxEXPAND | wxALL, 4);
    sizer->Add(m_canvas, 1, wxEXPAND | wxALL, 4);
    sizer->Add(m_props, 0, wxEXPAND | wxALL, 4);
    SetSizer(sizer);

    Layout();
}

void UiMainFrame::BindCommands() {
    // 菜单与工具栏走同一批处理函数
    Bind(wxEVT_MENU, &UiMainFrame::OnNew, this, wxID_NEW);
    Bind(wxEVT_MENU, &UiMainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &UiMainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &UiMainFrame::OnSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &UiMainFrame::OnExportNetlist, this, ID_EXPORT);
    Bind(wxEVT_MENU, &UiMainFrame::OnDeleteSelection, this, ID_DELETE_SEL);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);

    Bind(wxEVT_TOOL, &UiMainFrame::OnNew, this, wxID_NEW);
    Bind(wxEVT_TOOL, &UiMainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_TOOL, &UiMainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_TOOL, &UiMainFrame::OnExportNetlist, this, ID_EXPORT);
}

// ---------------------------------------------------------------------------
// 文件菜单
// ---------------------------------------------------------------------------

std::string UiMainFrame::Utf8Path(const wxString& path) const {
    return std::string(path.ToUTF8().data());
}

void UiMainFrame::OnNew(wxCommandEvent&) {
    m_model.loadFrom(editor::Schematic{});
    m_path.clear();
    m_canvas->OnDocumentReplaced();
    UpdateTitle();
    RefreshCounts();
    SetStatusText(U8("已新建空白原理图 —— 从左侧元件库选元件,点画布放置"), 0);
}

void UiMainFrame::OnOpen(wxCommandEvent&) {
    wxFileDialog dlg(this, U8("打开原理图"), "", "",
                     U8("原理图 JSON (*.json)|*.json|所有文件 (*.*)|*.*"),
                     wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }

    editor::Schematic loaded;
    if (!m_io.load(loaded, Utf8Path(dlg.GetPath()))) {
        SetStatusText(U8("打开失败(文件不存在或 JSON 格式不对):") + dlg.GetPath(), 0);
        return;
    }

    m_model.loadFrom(loaded);
    m_path = dlg.GetPath();
    m_canvas->OnDocumentReplaced();
    UpdateTitle();
    RefreshCounts();
    SetStatusText(U8("已打开:") + m_path + U8("  (开关默认关闭)"), 0);
}

void UiMainFrame::OnSave(wxCommandEvent&) {
    if (m_path.empty()) {
        wxCommandEvent dummy;
        OnSaveAs(dummy);
        return;
    }
    SaveDocument(m_path);
}

void UiMainFrame::OnSaveAs(wxCommandEvent&) {
    wxFileDialog dlg(this, U8("另存为"), "", "demo.json",
                     U8("原理图 JSON (*.json)|*.json"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }
    SaveDocument(dlg.GetPath());
}

bool UiMainFrame::SaveDocument(const wxString& path) {
    if (!m_io.save(m_model.data(), Utf8Path(path))) {
        SetStatusText(U8("保存失败:") + path, 0);
        return false;
    }
    m_path = path;
    UpdateTitle();
    SetStatusText(U8("已保存:") + path, 0);
    return true;
}

void UiMainFrame::OnExportNetlist(wxCommandEvent&) {
    if (m_model.data().components.empty()) {
        SetStatusText(U8("导出失败:原理图是空的,先放几个元件"), 0);
        return;
    }
    wxFileDialog dlg(this, U8("导出网表"), "", "demo.net",
                     U8("KiCad 网表 (*.net)|*.net"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK) {
        return;
    }
    if (!m_io.exportNetlist(m_model.data(), Utf8Path(dlg.GetPath()))) {
        SetStatusText(U8("导出失败:") + dlg.GetPath(), 0);
        return;
    }
    SetStatusText(U8("已导出网表:") + dlg.GetPath(), 0);
}

void UiMainFrame::OnDeleteSelection(wxCommandEvent&) {
    if (!m_canvas->DeleteSelection()) {
        SetStatusText(U8("没有选中任何对象"), 0);
    }
}

// ---------------------------------------------------------------------------
// 标题与计数
// ---------------------------------------------------------------------------

void UiMainFrame::UpdateTitle() {
    const wxString name = m_path.empty() ? U8("未命名") : wxFileName(m_path).GetFullName();
    // 注意:非 ASCII 文本必须走 U8()(FromUTF8)。窄字面量在非 UTF-8 locale 下会被吞成空串,
    // 之前这里写成 "Circuit Editor — " 就导致窗口标题只剩下文件名。
    SetTitle(U8("Circuit Editor —— ") + name);
}

void UiMainFrame::RefreshCounts() {
    const editor::Schematic& s = m_model.data();
    SetStatusText(wxString::Format(U8("元件 %d | 导线 %d | 网络 %d"),
                                   static_cast<int>(s.components.size()),
                                   static_cast<int>(s.wires.size()),
                                   static_cast<int>(s.nets.size())),
                  1);
}
