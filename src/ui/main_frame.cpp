#include "ui/main_frame.h"

#include <wx/wx.h>
#include <wx/artprov.h>

#include "ui/canvas_panel.h"
#include "ui/component_palette.h"
#include "ui/property_panel.h"

UiMainFrame::UiMainFrame()
    : wxFrame(nullptr, wxID_ANY, "Circuit Editor",
              wxDefaultPosition, wxSize(1000, 680)) {
    SetMinSize(wxSize(800, 560));

    BuildMenuBar();
    BuildToolBar();
    BuildStatusBar();
    BuildLayout();

    Centre();
}

void UiMainFrame::BuildMenuBar() {
    auto* fileMenu = new wxMenu;
    fileMenu->Append(wxID_EXIT, "E&xit\tCtrl+Q", "Quit the application");

    auto* bar = new wxMenuBar;
    bar->Append(fileMenu, "&File");
    SetMenuBar(bar);

    Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
}

void UiMainFrame::BuildToolBar() {
    auto* tb = CreateToolBar();
    tb->AddTool(wxID_NEW, "New",
                wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR),
                "New circuit");
    tb->Realize();
}

void UiMainFrame::BuildStatusBar() {
    CreateStatusBar();
    SetStatusText("Ready");
}

void UiMainFrame::BuildLayout() {
    m_palette = new ComponentPalette(this);
    m_canvas = new CanvasPanel(this);
    m_props = new PropertyPanel(this);

    // 左/右固定宽度,中栏占满剩余空间
    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(m_palette, 0, wxEXPAND | wxALL, 4);
    sizer->Add(m_canvas, 1, wxEXPAND | wxALL, 4);
    sizer->Add(m_props, 0, wxEXPAND | wxALL, 4);
    SetSizer(sizer);

    Layout();
}
