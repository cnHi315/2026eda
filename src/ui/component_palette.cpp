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
class FragmentItemData : public wxTreeItemData {
public:
    explicit FragmentItemData(SubcircuitLibrary::Item value) : item(std::move(value)) {}
    SubcircuitLibrary::Item item;
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

void ComponentPalette::SetSubcircuitCallback(SubcircuitCallback cb) { m_onSubcircuit = std::move(cb); }

void ComponentPalette::FillSubcircuits(const std::vector<SubcircuitLibrary::Item>& items) {
    const auto root = m_tree->GetRootItem();
    wxTreeItemIdValue cookie;
    wxTreeItemId category;
    for (auto child = m_tree->GetFirstChild(root, cookie); child.IsOk(); child = m_tree->GetNextChild(root, cookie)) {
        if (m_tree->GetItemText(child) == U8("自定义子电路")) { category = child; break; }
    }
    if (!category.IsOk()) category = m_tree->AppendItem(root, U8("自定义子电路"));
    else {
        const auto selected = m_tree->GetSelection();
        if (selected.IsOk() && m_tree->GetItemParent(selected) == category) ClearSelection();
        m_tree->DeleteChildren(category);
    }
    for (const auto& item : items) {
        const auto child = m_tree->AppendItem(category, item.name);
        m_tree->SetItemData(child, new FragmentItemData(item));
    }
    m_tree->Expand(category);
}

void ComponentPalette::ClearSelection() {
    if (m_tree == nullptr) {
        return;
    }
    // Unselect() 会触发 wxEVT_TREE_SEL_CHANGED → m_onType("") → 画布再次清空放置模式,
    // 但画布那边有 "值没变就不回调" 的短路,所以不会来回震荡。
    if (m_tree->GetSelection().IsOk()) {
        m_tree->Unselect();
    }
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
    if (evt.GetItem().IsOk()) {
        if (auto* fragment = dynamic_cast<FragmentItemData*>(m_tree->GetItemData(evt.GetItem()))) {
            if (m_onSubcircuit) m_onSubcircuit(fragment->item.schematic, fragment->item.name.ToStdString(wxConvUTF8));
            evt.Skip(); return;
        }
    }
    if (m_onType) {
        auto* data = evt.GetItem().IsOk() ? dynamic_cast<TypeItemData*>(m_tree->GetItemData(evt.GetItem())) : nullptr;
        m_onType(data == nullptr ? std::string{} : data->type);
    }
    evt.Skip();
}
