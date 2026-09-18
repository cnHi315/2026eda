#include "ui/property_panel.h"

#include <wx/propgrid/propgrid.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace {
wxString U8(const char* s) { return wxString::FromUTF8(s); }
} // namespace

PropertyPanel::PropertyPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    SetMinSize(wxSize(260, -1));

    auto* title = new wxStaticText(this, wxID_ANY, U8("元件属性"));

    m_grid = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                wxPG_DEFAULT_STYLE | wxPG_SPLITTER_AUTO_CENTER);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(title, 0, wxALL, 4);
    sizer->Add(m_grid, 1, wxEXPAND | wxALL, 4);
    SetSizer(sizer);

    FillPlaceholder();
}

void PropertyPanel::FillPlaceholder() {
    m_grid->Append(new wxPropertyCategory(U8("元件")));
    m_grid->Append(new wxStringProperty("id", wxPG_LABEL, wxEmptyString));
    m_grid->Append(new wxStringProperty(U8("类型"), wxPG_LABEL, wxEmptyString));
    m_grid->Append(new wxStringProperty(U8("名称"), wxPG_LABEL, wxEmptyString));

    m_grid->Append(new wxPropertyCategory(U8("几何")));
    m_grid->Append(new wxStringProperty(U8("位置"), wxPG_LABEL, wxEmptyString));
    m_grid->Append(new wxStringProperty(U8("旋转"), wxPG_LABEL, wxEmptyString));
}
