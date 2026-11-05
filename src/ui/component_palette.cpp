#include "ui/component_palette.h"

#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/treectrl.h>
#include <wx/generic/treectlg.h>
#include <wx/dcbuffer.h>
#include <algorithm>
#include <functional>

#include "components/component_library.h"
#include "ui/ui_theme.h"

namespace {

wxString U8(const char* s) { return wxString::FromUTF8(s); }

// Keep the tree data/selection API, but share complete row bounds between
// painting and pointer hit testing. The stock generic tree excludes y == row.y.
class PaletteTree final : public wxGenericTreeCtrl {
public:
    explicit PaletteTree(wxWindow* parent)
        : wxGenericTreeCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxTR_HAS_BUTTONS | wxTR_HIDE_ROOT | wxTR_SINGLE | wxTR_NO_LINES | wxBORDER_NONE) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        Bind(wxEVT_PAINT, &PaletteTree::Paint, this);
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent& e) { Choose(e, false); });
        Bind(wxEVT_LEFT_DCLICK, [this](wxMouseEvent& e) { Choose(e, true); });
        Bind(wxEVT_LEFT_UP, [](wxMouseEvent&) {});
        Bind(wxEVT_TREE_DELETE_ITEM, [this](wxTreeEvent& e) { m_hover = {}; e.Skip(); });
        Bind(wxEVT_MOTION, [this](wxMouseEvent& e) {
            int flags; const auto item = HitTest(e.GetPosition(), flags);
            if (item != m_hover) { m_hover = item; Refresh(); }
        });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) { m_hover = {}; Refresh(); });
    }
protected:
    void OnInternalIdle() override {
        wxWindow::OnInternalIdle();
        m_lineHeight = std::max(m_lineHeight, 36);
        if (m_dirty) DoDirtyProcessing();
    }
    wxTreeItemId DoTreeHitTest(const wxPoint& point, int& flags) const override {
        flags = wxTREE_HITTEST_NOWHERE;
        if (!GetClientRect().Contains(point)) return {};
        auto* self = const_cast<PaletteTree*>(this);
        self->m_lineHeight = std::max(self->m_lineHeight, 36);
        self->CalculatePositions();
        for (const auto& item : Rows()) {
            wxRect row;
            if (GetBoundingRect(item, row) && row.Contains(point)) {
                flags = HasChildren(item) && point.x < 18 ? wxTREE_HITTEST_ONITEMBUTTON : wxTREE_HITTEST_ONITEMLABEL;
                return item;
            }
        }
        return {};
    }
private:
    wxTreeItemId m_hover;
    std::vector<wxTreeItemId> Rows() const {
        std::vector<wxTreeItemId> items;
        std::function<void(wxTreeItemId)> append = [&](wxTreeItemId parent) {
            wxTreeItemIdValue cookie;
            for (auto item=GetFirstChild(parent,cookie); item.IsOk(); item=GetNextChild(parent,cookie)) {
                items.push_back(item); if (IsExpanded(item)) append(item);
            }
        };
        if (GetRootItem().IsOk()) append(GetRootItem());
        return items;
    }
    void Choose(wxMouseEvent& event, bool doubleClick) {
        if (!IsEnabled()) return;
        SetFocus(); int flags; const auto item=HitTest(event.GetPosition(),flags);
        if (!item.IsOk()) return;
        if (HasChildren(item) && ((flags & wxTREE_HITTEST_ONITEMBUTTON) || doubleClick)) Toggle(item);
        else SelectItem(item);
        Refresh();
    }
    void Paint(wxPaintEvent&) {
        m_lineHeight=std::max(m_lineHeight,36);CalculatePositions();
        wxAutoBufferedPaintDC dc(this);dc.SetBackground(wxBrush(ui_theme::Surface()));dc.Clear();
        for (const auto& item : Rows()) {
            wxRect row; if (!GetBoundingRect(item,row) || !row.Intersects(GetClientRect())) continue;
            const bool selected=IsSelected(item), hovered=item==m_hover;
            const auto fill=selected?ui_theme::Selection():hovered?ui_theme::Hover():ui_theme::Surface();
            ui_theme::DrawSurface(dc,wxRect(row).Deflate(2,1),fill,8,HasFocus()&&GetSelection()==item?ui_theme::Focus():wxNullColour);
            const auto font=GetItemFont(item);dc.SetFont(font.IsOk()?font:GetFont());
            dc.SetTextForeground(GetItemTextColour(item).IsOk()?GetItemTextColour(item):ui_theme::Ink());
            int indent=20;
            if (GetItemParent(item)!=GetRootItem()) indent=26;
            const auto label=wxControl::Ellipsize(GetItemText(item),dc,wxELLIPSIZE_END,std::max(0,row.width-indent-8));
            dc.DrawText(label,indent,row.y+(row.height-dc.GetTextExtent(label).y)/2);
            if (HasChildren(item)) {
                dc.SetPen(wxPen(ui_theme::Muted(),1));
                const int y=row.y+row.height/2;
                if (IsExpanded(item)) {dc.DrawLine(7,y-2,11,y+2);dc.DrawLine(11,y+2,15,y-2);}
                else {dc.DrawLine(9,y-4,13,y);dc.DrawLine(13,y,9,y+4);}
            }
        }
    }
};

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
    SetMinSize(wxSize(196, -1));
    ui_theme::StyleCard(this);

    m_tree = new PaletteTree(this);
    m_tree->SetBackgroundColour(ui_theme::Surface());
    m_tree->SetForegroundColour(ui_theme::Ink());
    m_tree->SetFont(ui_theme::Font());
    m_tree->SetIndent(14);
    m_tree->SetSpacing(9);
    m_tree->Bind(wxEVT_TREE_SEL_CHANGED, &ComponentPalette::OnSelectionChanged, this);

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(ui_theme::SectionHeader(this, U8("元件库"), U8("内置元件与可复用子电路")),
               0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    sizer->Add(ui_theme::Rule(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 1);
    sizer->Add(m_tree, 1, wxEXPAND | wxALL, 8);
    auto* help = new wxStaticText(this, wxID_ANY, U8("选择元件后，点击画布放置\nEsc 退出放置"));
    help->SetFont(ui_theme::Font(16));
    help->SetForegroundColour(ui_theme::Muted());
    sizer->Add(ui_theme::Rule(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 1);
    sizer->Add(help, 0, wxALL, 12);
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
    m_tree->SetItemFont(category, ui_theme::Font());
    m_tree->SetItemTextColour(category, ui_theme::Muted());
    for (const auto& item : items) {
        const auto child = m_tree->AppendItem(category, item.name);
        m_tree->SetItemFont(child, ui_theme::Font());
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
        m_tree->SetItemFont(item, ui_theme::Font());
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
