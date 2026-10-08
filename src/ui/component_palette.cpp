#include "ui/component_palette.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>

#include "components/component_library.h"

namespace {

wxString U8(const char* s) { return wxString::FromUTF8(s); }

/// 挂在树节点上的类型串(树的显示文本是中文 displayName)。
class TypeItemData : public wxTreeItemData {
public:
    explicit TypeItemData(std::string t) : type(std::move(t)) {}
    std::string type;
};

} // namespace

ComponentPalette::ComponentPalette(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    SetMinSize(wxSize(200, -1));

    auto* title = new wxStaticText(this, wxID_ANY, U8("元件库"));

    m_tree = new wxTreeCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE);
    m_tree->Bind(wxEVT_TREE_SEL_CHANGED, &ComponentPalette::OnSelectionChanged, this);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(title, 0, wxALL, 4);
    sizer->Add(m_tree, 1, wxEXPAND | wxALL, 4);
    sizer->Add(new wxStaticText(this, wxID_ANY, U8("选中一项后,点画布放置")),
               0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
    SetSizer(sizer);
}

void ComponentPalette::SetTypeCallback(TypeCallback cb) {
    m_onType = std::move(cb);
}

void ComponentPalette::FillFromLibrary(const editor::ComponentLibrary& lib) {
    m_tree->DeleteAllItems();
    const wxTreeItemId root = m_tree->AddRoot(U8("元件库"));

    for (const std::string& type : lib.types()) {
        const wxString label = wxString::FromUTF8(lib.displayName(type).c_str());
        const wxTreeItemId item = m_tree->AppendItem(root, label);
        m_tree->SetItemData(item, new TypeItemData(type));
    }
    m_tree->ExpandAll();
}

void ComponentPalette::OnSelectionChanged(wxTreeEvent& evt) {
    if (m_onType) {
        auto* data = dynamic_cast<TypeItemData*>(m_tree->GetItemData(evt.GetItem()));
        m_onType(data == nullptr ? std::string{} : data->type);
    }
    evt.Skip();
}
