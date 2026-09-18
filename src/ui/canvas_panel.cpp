#include "ui/canvas_panel.h"

#include <wx/sizer.h>
#include <wx/stattext.h>

namespace {
wxString U8(const char* s) { return wxString::FromUTF8(s); }
} // namespace

CanvasPanel::CanvasPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxFULL_REPAINT_ON_RESIZE) {
    SetBackgroundColour(*wxWHITE);
    SetMinSize(wxSize(400, 300));

    auto* hint = new wxStaticText(
        this, wxID_ANY, U8("绘图区(阶段二实现网格与元件绘制)"));
    hint->SetForegroundColour(wxColour(150, 150, 150));

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->AddStretchSpacer(1);
    sizer->Add(hint, 0, wxALIGN_CENTER | wxALL, 8);
    sizer->AddStretchSpacer(1);
    SetSizer(sizer);
}
