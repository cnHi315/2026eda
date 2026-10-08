#include "ui/property_panel.h"

#include <wx/propgrid/propgrid.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "contract/data_model.h"

namespace {

wxString U8(const char* s) { return wxString::FromUTF8(s); }

/// 只读的字符串属性行。
wxPGProperty* RO(wxPropertyGrid* grid, const wxString& label, const wxString& value) {
    auto* prop = new wxStringProperty(label, wxPG_LABEL, value);
    grid->Append(prop);
    grid->SetPropertyReadOnly(prop);
    return prop;
}

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

    ShowComponent(nullptr);
}

void PropertyPanel::ShowComponent(const editor::Component* c) {
    m_grid->Clear();

    m_grid->Append(new wxPropertyCategory(U8("元件")));
    if (c == nullptr) {
        RO(m_grid, U8("选中"), U8("(未选中)"));
        return;
    }

    RO(m_grid, "id", wxString::FromUTF8(c->id.c_str()));
    RO(m_grid, U8("类型"), wxString::FromUTF8(c->type.c_str()));
    RO(m_grid, U8("名称"), wxString::FromUTF8(c->name.c_str()));

    m_grid->Append(new wxPropertyCategory(U8("几何")));
    RO(m_grid, U8("位置"), wxString::Format("(%d, %d)", c->pos.x, c->pos.y));
    RO(m_grid, U8("旋转"), wxString::Format(U8("%d°"), c->rotation));

    m_grid->Append(new wxPropertyCategory(U8("引脚")));
    RO(m_grid, U8("数量"), wxString::Format("%zu", c->pins.size()));
    for (size_t i = 0; i < c->pins.size(); ++i) {
        const editor::PinDescriptor& p = c->pins[i];
        const wxString dir = p.direction == editor::PinDirection::Input ? U8("输入") : U8("输出");
        RO(m_grid, wxString::Format("[%zu] ", i) + wxString::FromUTF8(p.name.c_str()),
           dir + wxString::Format("  rel(%d, %d)", p.relPos.x, p.relPos.y));
    }
}
