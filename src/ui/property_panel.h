#pragma once
// B: read-only component information, grouped into separate white cards.
#include <wx/panel.h>
#include <utility>
#include <vector>

namespace editor { struct Component; }
class wxScrolledWindow;
class wxBoxSizer;
class wxStaticText;

class PropertyPanel : public wxPanel {
public:
    explicit PropertyPanel(wxWindow* parent);
    void ShowComponent(const editor::Component* c);
    void ShowSelectionCount(size_t count);
private:
    using Rows = std::vector<std::pair<wxString, wxString>>;
    using Cards = std::vector<std::pair<wxString, Rows>>;
    void PresentCards(const Cards& cards);
    void BeginCards();
    void AddCard(const wxString& title, const Rows& rows);
    void EndCards();
    void Reflow();
    wxScrolledWindow* m_content = nullptr;
    wxBoxSizer* m_cards = nullptr;
    std::vector<std::pair<wxStaticText*, wxString>> m_values;
    std::vector<wxString> m_structure;
    int m_wrapWidth = -1;
};
