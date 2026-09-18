#include "ui/component_palette.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

namespace {
wxString U8(const char* s) { return wxString::FromUTF8(s); }
} // namespace

ComponentPalette::ComponentPalette(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    SetMinSize(wxSize(200, -1));

    auto* title = new wxStaticText(this, wxID_ANY, U8("元件库"));

    m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(title, 0, wxALL, 4);
    sizer->Add(m_tree, 1, wxEXPAND | wxALL, 4);
    SetSizer(sizer);

    FillPlaceholder();
}

void ComponentPalette::FillPlaceholder() {
    // 阶段一占位;阶段四换成 ComponentLibrary::types() / displayName()
    static const char* kTypes[] = {"AND", "OR", "NOT", "SWITCH", "LED"};

    const wxTreeItemId root = m_tree->AddRoot(U8("元件库"));
    for (const char* type : kTypes) {
        m_tree->AppendItem(root, U8(type));
    }
    m_tree->ExpandAll();
}
