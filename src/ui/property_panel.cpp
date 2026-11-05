#include "ui/property_panel.h"

#include <wx/dcbuffer.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <algorithm>

#include "contract/data_model.h"
#include "ui/ui_theme.h"

namespace {
wxString U8(const char* s) { return wxString::FromUTF8(s); }
// wxStaticText::Wrap leaves an unbroken name wider than the available column.
// Break long tokens as well as Chinese text, retaining the full value in a tip.
wxString WrapValue(wxStaticText* control, const wxString& text, int width) {
    wxClientDC dc(control); dc.SetFont(control->GetFont());
    wxString output, line;
    for (const auto ch : text) {
        if (ch == '\n') { output += line + "\n"; line.clear(); continue; }
        if (!line.empty() && dc.GetTextExtent(line + ch).x > width) {
            output += line + "\n"; line.clear();
        }
        line += ch;
    }
    return output + line;
}
}

PropertyPanel::PropertyPanel(wxWindow* parent) : wxPanel(parent, wxID_ANY) {
    SetMinSize(wxSize(248, -1));
    ui_theme::StyleCard(this);
    m_content = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxVSCROLL | wxBORDER_NONE);
    m_content->SetBackgroundColour(ui_theme::Surface());
    m_content->SetScrollRate(0, 12);
    m_cards = new wxBoxSizer(wxVERTICAL);
    m_content->SetSizer(m_cards);
    m_content->Bind(wxEVT_SIZE, [this](wxSizeEvent& e) { Reflow(); e.Skip(); });

    auto* layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(ui_theme::SectionHeader(this, U8("元件属性"), U8("查看选中元件的信息")),
                0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);
    layout->Add(ui_theme::Rule(this), 0, wxEXPAND | wxLEFT | wxRIGHT, 1);
    layout->Add(m_content, 1, wxEXPAND | wxALL, 12);
    SetSizer(layout);
    ShowComponent(nullptr);
}

void PropertyPanel::BeginCards() {
    Freeze();
    m_values.clear();
    m_cards->Clear(true);
    m_wrapWidth = -1;
}

void PropertyPanel::AddCard(const wxString& title, const Rows& rows) {
    auto* card = new wxPanel(m_content, wxID_ANY);
    card->SetName("B-property-card-" + title);
    card->SetBackgroundStyle(wxBG_STYLE_PAINT);
    card->SetBackgroundColour(ui_theme::Surface());
    card->Bind(wxEVT_PAINT, [card](wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(card);
        dc.SetBackground(wxBrush(ui_theme::Surface())); dc.Clear();
        ui_theme::DrawSurface(dc, card->GetClientRect(), ui_theme::Surface(), 12, ui_theme::Border());
    });
    auto* contents = new wxBoxSizer(wxVERTICAL);
    auto* heading = new wxStaticText(card, wxID_ANY, title);
    heading->SetFont(ui_theme::Font(16, true)); heading->SetForegroundColour(ui_theme::Ink());
    contents->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, 12);
    contents->AddSpacer(10);
    // Two aligned text columns, without cells, separators or shaded headers.
    auto* fields = new wxFlexGridSizer(2, 8, 12);
    fields->AddGrowableCol(1);
    for (const auto& row : rows) {
        auto* label = new wxStaticText(card, wxID_ANY, row.first);
        label->SetFont(ui_theme::Font(16)); label->SetForegroundColour(ui_theme::Ink());
        label->SetMinSize(wxSize(56, -1));
        auto* value = new wxStaticText(card, wxID_ANY, row.second);
        value->SetName("B-property-value-" + row.first);
        value->SetFont(ui_theme::Font(16)); value->SetForegroundColour(ui_theme::Ink());
        value->SetMinSize(wxSize(0, -1)); value->SetToolTip(row.second);
        m_values.emplace_back(value, row.second);
        fields->Add(label, 0, wxALIGN_TOP);
        fields->Add(value, 1, wxEXPAND);
    }
    contents->Add(fields, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    card->SetSizer(contents);
    if (!m_cards->IsEmpty()) m_cards->AddSpacer(10);
    m_cards->Add(card, 0, wxEXPAND);
}

void PropertyPanel::EndCards() {
    Reflow();
    m_content->Scroll(0, 0);
    Thaw();
    Refresh();
}

void PropertyPanel::Reflow() {
    if (!m_content || !m_cards) return;
    const int width = std::max(32, m_content->GetClientSize().x - 24 - 56 - 12);
    if (width == m_wrapWidth) return;
    m_wrapWidth = width;
    for (auto& value : m_values) {
        value.first->SetLabel(WrapValue(value.first, value.second, width));
    }
    m_content->Layout();
    m_content->FitInside();
}

void PropertyPanel::PresentCards(const Cards& cards) {
    std::vector<wxString> structure;
    std::vector<wxString> values;
    for (const auto& card : cards) {
        structure.push_back("card:" + card.first);
        for (const auto& row : card.second) { structure.push_back("row:" + row.first); values.push_back(row.second); }
    }
    if (structure == m_structure && values.size() == m_values.size()) {
        bool changed = false;
        for (size_t i=0; i<values.size(); ++i) {
            if (m_values[i].second != values[i]) {
                m_values[i].second = values[i]; m_values[i].first->SetToolTip(values[i]); changed = true;
            }
        }
        if (changed) { m_wrapWidth = -1; Reflow(); }
        return;
    }
    BeginCards();
    m_structure = std::move(structure);
    for (const auto& card : cards) AddCard(card.first, card.second);
    EndCards();
}

void PropertyPanel::ShowComponent(const editor::Component* c) {
    if (!c) { PresentCards({{U8("元件"), {{U8("选中"), U8("尚未选择元件")}}}}); return; }
    Rows pins{{U8("数量"), wxString::Format("%zu", c->pins.size())}};
    for (size_t i = 0; i < c->pins.size(); ++i) {
        const auto& pin = c->pins[i];
        const auto direction = pin.direction == editor::PinDirection::Input ? U8("输入") : U8("输出");
        pins.emplace_back(wxString::Format("[%zu] ", i) + wxString::FromUTF8(pin.name),
                          direction + wxString::Format("  rel(%d, %d)", pin.relPos.x, pin.relPos.y));
    }
    PresentCards({
        {U8("元件"), {{"id", wxString::FromUTF8(c->id)}, {U8("类型"), wxString::FromUTF8(c->type)}, {U8("名称"), wxString::FromUTF8(c->name)}}},
        {U8("几何"), {{U8("位置"), wxString::Format("(%d, %d)", c->pos.x, c->pos.y)}, {U8("旋转"), wxString::Format(U8("%d°"), c->rotation)}}},
        {U8("引脚"), pins}
    });
}

void PropertyPanel::ShowSelectionCount(size_t count) {
    PresentCards({{U8("多选"), {{U8("元件数量"), wxString::Format("%zu", count)},
                              {U8("移动"), U8("拖动选中元件，整组移动")},
                              {U8("删除"), U8("Delete 删除整组")},
                              {U8("封存"), U8("编辑 → 封存为子电路")}}}});
}
