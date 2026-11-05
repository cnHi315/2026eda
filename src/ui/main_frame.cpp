#include "ui/main_frame.h"

#include <wx/wx.h>
#include <wx/filename.h>

#include <string>

#include "ui/canvas_panel.h"
#include "ui/component_palette.h"
#include "ui/property_panel.h"
#include "ui/ui_theme.h"

namespace {

wxString U8(const char* s) { return wxString::FromUTF8(s); }

enum {
    ID_EXPORT = wxID_HIGHEST + 1,
    ID_DELETE_SEL,
    ID_SAVE_SUBCIRCUIT,
    ID_RELOAD_SUBCIRCUITS,
    ID_FILE_MENU,
    ID_EDIT_MENU,
    ID_VIEW_MENU,
    ID_SHOW_TOOLBAR,
    ID_SHOW_PROPERTIES,
    ID_SNAP_1, ID_SNAP_5, ID_SNAP_10, ID_SNAP_20,
};

} // namespace

UiMainFrame::UiMainFrame(const wxString& subcircuitDirectory)
    : wxFrame(nullptr, wxID_ANY, "Circuit Editor",
              wxDefaultPosition, wxSize(1100, 800)),
      m_subcircuits(subcircuitDirectory.empty() ? wxFileName::DirName(wxGetCwd()).GetPathWithSep() + "subcircuits"
                                               : subcircuitDirectory) {
    SetMinSize(wxSize(1000, 680));
    SetBackgroundColour(ui_theme::Workspace());
    SetForegroundColour(ui_theme::Ink());
    SetFont(ui_theme::Font());

    BuildMenuBar();
    BuildToolBar();
    BuildStatusBar();
    BuildLayout();
    BindCommands();

    Centre();
    UpdateTitle();
    RefreshCounts();
    SetStatusText(U8("就绪 —— 从左侧元件库选元件,点画布放置"));
    ReloadSubcircuits();
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
    editMenu->Append(wxID_UNDO, U8("撤销(&U)\tCtrl+Z"), U8("回退上一步编辑"));
    editMenu->Append(wxID_REDO, U8("重做(&R)\tCtrl+Y"), U8("前进到下一步"));
    editMenu->AppendSeparator();
    editMenu->Append(ID_DELETE_SEL, U8("删除选中(&D)\tDel"),
                     U8("删除选中的元件或导线"));
    editMenu->AppendSeparator();
    editMenu->Append(ID_SAVE_SUBCIRCUIT, U8("封存为子电路(&B)..."));
    editMenu->Append(ID_RELOAD_SUBCIRCUITS, U8("刷新子电路库(&L)"));

    auto* viewMenu = new wxMenu;
    viewMenu->AppendCheckItem(ID_SHOW_TOOLBAR, U8("工具栏(&T)"))->Check(true);
    viewMenu->AppendCheckItem(ID_SHOW_PROPERTIES, U8("元件属性(&P)"))->Check(true);
    viewMenu->AppendSeparator();
    viewMenu->AppendRadioItem(ID_SNAP_1,U8("吸附 1 单位(&1)"))->Check(true);
    viewMenu->AppendRadioItem(ID_SNAP_5,U8("吸附 5 单位(&5)"));
    viewMenu->AppendRadioItem(ID_SNAP_10,U8("吸附 10 单位(&0)"));
    viewMenu->AppendRadioItem(ID_SNAP_20,U8("吸附 20 单位(&2)"));

    auto* bar = new wxMenuBar;
    bar->Append(fileMenu, U8("文件(&F)"));
    bar->Append(editMenu, U8("编辑(&E)"));
    bar->Append(viewMenu, U8("视图(&V)"));
    SetMenuBar(bar);
    // Keep the command definitions for accelerators and menu consumers; the
    // visible menu triggers/popups share our controlled grey appearance.
    bar->Hide();
    const wxAcceleratorEntry entries[] = {
        {wxACCEL_CTRL, 'N', wxID_NEW}, {wxACCEL_CTRL, 'O', wxID_OPEN},
        {wxACCEL_CTRL, 'S', wxID_SAVE}, {wxACCEL_CTRL, 'E', ID_EXPORT},
        {wxACCEL_CTRL, 'Z', wxID_UNDO}, {wxACCEL_CTRL, 'Y', wxID_REDO},
        {wxACCEL_CTRL, 'Q', wxID_EXIT}, {wxACCEL_ALT, 'F', ID_FILE_MENU},
        {wxACCEL_ALT, 'E', ID_EDIT_MENU}, {wxACCEL_ALT, 'V', ID_VIEW_MENU},
        {wxACCEL_NORMAL, WXK_DELETE, ID_DELETE_SEL}
    };
    SetAcceleratorTable(wxAcceleratorTable(sizeof(entries)/sizeof(entries[0]),entries));
}

void UiMainFrame::BuildToolBar() {
    m_topBar = new wxPanel(this, wxID_ANY);
    m_topBar->SetName("B-top-bar");
    m_topBar->SetBackgroundColour(ui_theme::Surface());
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    const int menuIds[] = {ID_FILE_MENU, ID_EDIT_MENU, ID_VIEW_MENU};
    const char* menuLabels[] = {"文件", "编辑", "视图"};
    for (int i = 0; i < 3; ++i) {
        auto* button = new ui_theme::ActionButton(m_topBar, menuIds[i], U8(menuLabels[i]), this, wxSize(64,36));
        row->Add(button, 0, wxRIGHT, 8);
        Bind(wxEVT_MENU, [this,button,i](wxCommandEvent&) {
            ui_theme::ShowCommandMenu(this, GetMenuBar()->GetMenu(i), button);
        }, menuIds[i]);
    }
    row->AddStretchSpacer();
    m_commandToolbar = new wxPanel(m_topBar, wxID_ANY);
    m_commandToolbar->SetName("B-command-toolbar");
    m_commandToolbar->SetBackgroundColour(ui_theme::Surface());
    auto* commands = new wxBoxSizer(wxHORIZONTAL);
    const auto add = [this,commands](int id, const wxString& label, const wxString& help) {
        if (!commands->IsEmpty()) commands->AddSpacer(8);
        auto* button = new ui_theme::ActionButton(m_commandToolbar,id,label,this,wxSize(88,36));
        button->SetToolTip(help); commands->Add(button);
    };
    add(wxID_NEW,U8("新建"),U8("新建空白原理图 (Ctrl+N)"));
    add(wxID_OPEN,U8("打开"),U8("打开 JSON 原理图 (Ctrl+O)"));
    add(wxID_SAVE,U8("保存"),U8("保存 (Ctrl+S)"));
    add(ID_EXPORT,U8("导出网表"),U8("导出 KiCad 网表 (Ctrl+E)"));
    add(wxID_UNDO,U8("撤销"),U8("撤销 (Ctrl+Z)"));
    add(wxID_REDO,U8("重做"),U8("重做 (Ctrl+Y)"));
    m_commandToolbar->SetSizer(commands);
    row->Add(m_commandToolbar);
    auto* padded = new wxBoxSizer(wxVERTICAL);
    padded->Add(row, 0, wxEXPAND | wxALL, 10);
    m_topBar->SetSizer(padded);
}

void UiMainFrame::BuildStatusBar() {
    auto* status = CreateStatusBar(2, wxSTB_DEFAULT_STYLE | wxSTB_ELLIPSIZE_END);
    status->SetBackgroundColour(ui_theme::Surface());
    status->SetForegroundColour(ui_theme::Muted());
    status->SetFont(ui_theme::Font(16));
    status->SetMinHeight(28);
    const int kWidths[] = {-1, 260};
    SetStatusWidths(2, kWidths);
    SetStatusText(U8("就绪"), 0);
    SetStatusText(U8(""), 1);
}

void UiMainFrame::BuildLayout() {
    m_palette = new ComponentPalette(this);
    auto* workspace = new wxPanel(this, wxID_ANY);
    ui_theme::StyleCard(workspace);
    m_canvas = new CanvasPanel(workspace);
    m_props = new PropertyPanel(this);

    // —— 阶段四接线:画布 ↔ 模型 / 仿真;元件树 → 放置模式;选中 → 属性表 ——
    m_canvas->Attach(&m_model, &m_sim);
    m_palette->FillFromLibrary(m_lib);

    m_palette->SetTypeCallback([this](const std::string& type) {
        m_canvas->SetPlacementType(type);
    });
    m_palette->SetSubcircuitCallback([this](const editor::Schematic& schematic, const std::string& name) {
        m_canvas->SetSubcircuitPlacement(schematic, name);
    });
    m_canvas->SetStatusCallback([this](const wxString& text) { SetStatusText(text, 0); });
    m_canvas->SetSelectionCallback(
        [this](const editor::Component* c) {
            if (m_canvas->ActiveComponentCount() > 1) m_props->ShowSelectionCount(m_canvas->ActiveComponentCount());
            else m_props->ShowComponent(c);
        });
    m_canvas->SetChangedCallback([this]() { RefreshCounts(); });
    // 撤销 / 打开 / 新建 会把放置模式清掉,这里同步把元件树的高亮也清掉
    m_canvas->SetPlacementCallback([this](const std::string& type) {
        if (type.empty()) {
            m_palette->ClearSelection();
        }
    });

    auto* drawing = new wxBoxSizer(wxVERTICAL);
    drawing->Add(ui_theme::SectionHeader(workspace, U8("原理图"),
                                       wxEmptyString, &m_canvasHint),
                 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    drawing->Add(ui_theme::Rule(workspace), 0, wxEXPAND | wxLEFT | wxRIGHT, 1);
    drawing->Add(m_canvas, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    workspace->SetSizer(drawing);
    UpdateCanvasHint();

    // 左/右保留原生控件,中间的画布占满可用空间。
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_palette, 0, wxEXPAND | wxTOP | wxBOTTOM | wxLEFT, 10);
    sizer->AddSpacer(8);
    sizer->Add(workspace, 1, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 10);
    sizer->Add(m_props, 0, wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, 10);
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(m_topBar, 0, wxEXPAND);
    root->Add(sizer, 1, wxEXPAND);
    SetSizer(root);

    Layout();
}

void UiMainFrame::BindCommands() {
    // 菜单与工具栏走同一批处理函数
    Bind(wxEVT_MENU, &UiMainFrame::OnUndo, this, wxID_UNDO);
    Bind(wxEVT_MENU, &UiMainFrame::OnRedo, this, wxID_REDO);
    Bind(wxEVT_MENU, &UiMainFrame::OnNew, this, wxID_NEW);
    Bind(wxEVT_MENU, &UiMainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &UiMainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &UiMainFrame::OnSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &UiMainFrame::OnExportNetlist, this, ID_EXPORT);
    Bind(wxEVT_MENU, &UiMainFrame::OnDeleteSelection, this, ID_DELETE_SEL);
    Bind(wxEVT_MENU, &UiMainFrame::OnSaveSubcircuit, this, ID_SAVE_SUBCIRCUIT);
    Bind(wxEVT_MENU, &UiMainFrame::OnReloadSubcircuits, this, ID_RELOAD_SUBCIRCUITS);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);

    Bind(wxEVT_TOOL, &UiMainFrame::OnNew, this, wxID_NEW);
    Bind(wxEVT_TOOL, &UiMainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_TOOL, &UiMainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_TOOL, &UiMainFrame::OnExportNetlist, this, ID_EXPORT);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        const bool show = !m_commandToolbar->IsShown();
        m_commandToolbar->Show(show);
        GetMenuBar()->Check(ID_SHOW_TOOLBAR, show);
        m_topBar->Layout(); Layout();
    }, ID_SHOW_TOOLBAR);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) {
        const bool show = !m_props->IsShown();
        m_props->Show(show);
        GetMenuBar()->Check(ID_SHOW_PROPERTIES, show);
        Layout();
    }, ID_SHOW_PROPERTIES);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Check(m_commandToolbar->IsShown()); }, ID_SHOW_TOOLBAR);
    Bind(wxEVT_UPDATE_UI, [this](wxUpdateUIEvent& e) { e.Check(m_props->IsShown()); }, ID_SHOW_PROPERTIES);
    const int snapIds[] = {ID_SNAP_1, ID_SNAP_5, ID_SNAP_10, ID_SNAP_20};
    const int steps[] = {1,5,10,20};
    for(int i=0;i<4;++i) {
        Bind(wxEVT_MENU,[this,step=steps[i],id=snapIds[i]](wxCommandEvent&) {
            if(m_canvas->SetSnapStep(step)) { GetMenuBar()->Check(id,true); UpdateCanvasHint(); }
        },snapIds[i]);
        Bind(wxEVT_UPDATE_UI,[this,step=steps[i]](wxUpdateUIEvent& e) {e.Check(m_canvas->SnapStep()==step);},snapIds[i]);
    }

    // 撤销 / 重做:菜单与工具栏都按"有没有东西可撤"置灰
    Bind(wxEVT_TOOL, &UiMainFrame::OnUndo, this, wxID_UNDO);
    Bind(wxEVT_TOOL, &UiMainFrame::OnRedo, this, wxID_REDO);
    Bind(wxEVT_UPDATE_UI, &UiMainFrame::OnUpdateUndo, this, wxID_UNDO);
    Bind(wxEVT_UPDATE_UI, &UiMainFrame::OnUpdateRedo, this, wxID_REDO);
}

void UiMainFrame::UpdateCanvasHint() {
    if(m_canvasHint) {
        m_canvasHint->SetLabel(wxString::Format(U8("吸附 %d 单位 · Alt 自由移动 · 方向键微调 · 中键平移"), m_canvas->SnapStep()));
        m_canvasHint->GetParent()->Layout();
    }
}

void UiMainFrame::ReloadSubcircuits() {
    const auto skipped = m_subcircuits.Reload();
    m_palette->FillSubcircuits(m_subcircuits.Items());
    if (skipped) SetStatusText(wxString::Format(U8("已跳过 %zu 个无法读取或不兼容的子电路文件"), skipped), 0);
}

void UiMainFrame::OnReloadSubcircuits(wxCommandEvent&) {
    m_canvas->ClearPlacementType();
    ReloadSubcircuits();
}

void UiMainFrame::OnSaveSubcircuit(wxCommandEvent&) {
    editor::Schematic fragment;
    wxString error;
    if (!m_canvas->ExportSelection(&fragment, &error)) { SetStatusText(error, 0); return; }
    ui_theme::NameDialog dialog(this);
    if (dialog.ShowModal() != wxID_OK) return;
    wxString name = dialog.GetValue(); name.Trim().Trim(false);
    bool overwrite = false;
    if (wxFileName(m_subcircuits.Directory(), name + ".json").FileExists()) {
        ui_theme::ConfirmDialog confirm(this);
        overwrite = confirm.ShowModal() == wxID_YES;
        if (!overwrite) return;
    }
    if (!m_subcircuits.Save(name, fragment, overwrite, &error)) { SetStatusText(error, 0); return; }
    ReloadSubcircuits();
    SetStatusText(U8("已封存子电路：") + name, 0);
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
    m_canvas->ClearHistory();                    // 新文档不继承旧历史
    UpdateTitle();
    RefreshCounts();
    SetStatusText(U8("已新建空白原理图 —— 从左侧元件库选元件,点画布放置"), 0);
}

void UiMainFrame::OnOpen(wxCommandEvent&) {
    // "所有文件"放在第一项:默认就能选任意文件,便于验证"打开非法文件 → 打开失败"的失败路径
    //(之前默认过滤 JSON,坏文件在对话框里是灰的,根本选不到)。
    wxFileDialog dlg(this, U8("打开原理图"), "", "",
                     U8("所有文件 (*.*)|*.*|原理图 JSON (*.json)|*.json"),
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
    m_canvas->ClearHistory();                    // 打开新文件后撤销栈从零开始
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

void UiMainFrame::OnUndo(wxCommandEvent&) {
    if (!m_canvas->Undo()) {
        SetStatusText(U8("没有可撤销的操作"), 0);
    }
}

void UiMainFrame::OnRedo(wxCommandEvent&) {
    if (!m_canvas->Redo()) {
        SetStatusText(U8("没有可重做的操作"), 0);
    }
}

void UiMainFrame::OnUpdateUndo(wxUpdateUIEvent& evt) {
    evt.Enable(m_canvas != nullptr && m_canvas->CanUndo());
}

void UiMainFrame::OnUpdateRedo(wxUpdateUIEvent& evt) {
    evt.Enable(m_canvas != nullptr && m_canvas->CanRedo());
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
