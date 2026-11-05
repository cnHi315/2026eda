#pragma once
// B: application chrome only; circuit geometry and simulation colours stay separate.
#include <wx/wx.h>
#include <wx/graphics.h>
#include <wx/textdlg.h>

namespace ui_theme {
inline wxColour Workspace() { return wxColour(244, 245, 247); }
inline wxColour Surface() { return wxColour(255, 255, 255); }
inline wxColour Subtle() { return wxColour(246, 246, 246); }
inline wxColour Border() { return wxColour(213, 213, 213); }
inline wxColour Ink() { return wxColour(39, 39, 39); }
inline wxColour Muted() { return wxColour(100, 100, 100); }
inline wxColour Accent() { return wxColour(60, 60, 60); }
inline wxColour Selection() { return wxColour(233, 233, 233); }
inline wxColour Button() { return wxColour(248, 248, 248); }
inline wxColour Hover() { return wxColour(242, 242, 242); }
inline wxColour Pressed() { return wxColour(233, 233, 233); }
inline wxColour Disabled() { return wxColour(236, 236, 236); }
inline wxColour DisabledInk() { return wxColour(154, 154, 154); }
inline wxColour Focus() { return wxColour(133, 133, 133); }
inline wxFont Font(int pixelHeight = 18, bool bold = false) {
    wxFont font = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    font.SetPixelSize(wxSize(0, pixelHeight));
    font.SetWeight(bold ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL);
    return font;
}
// Each corner is a cubic with coincident controls at the outer corner. Its
// curvature tends to zero at both ends, so it joins the straight edges smoothly.
wxGraphicsPath ContinuousRect(wxGraphicsContext& gc, const wxRect2DDouble& rect, double radius);
void DrawSurface(wxDC& dc, const wxRect& rect, const wxColour& fill, double radius,
                 const wxColour& border = wxNullColour);
wxPanel* Rule(wxWindow* parent);
void StyleCard(wxPanel* panel);
wxPanel* SectionHeader(wxWindow* parent, const wxString& title, const wxString& subtitle, wxStaticText** captionControl = nullptr);

class ActionButton : public wxControl {
public:
    ActionButton(wxWindow* parent, int id, const wxString& label, wxWindow* target,
                 const wxSize& size = wxSize(88, 36),
                 wxEventType commandType = wxEVT_MENU);
    void Activate();
    void SetChecked(bool checked) { m_checked = checked; Refresh(); }
    bool IsChecked() const { return m_checked; }
    bool Enable(bool enable = true) override;
private:
    void Paint(wxPaintEvent&);
    void ResetPress();
    wxWindow* m_target;
    wxEventType m_commandType;
    bool m_checked = false;
    bool m_hover = false, m_pressed = false, m_keyPressed = false;
};
void ShowCommandMenu(wxWindow* target, wxMenu* menu, wxWindow* anchor);
class NameDialog : public wxTextEntryDialog {
public:
    explicit NameDialog(wxWindow* parent);
};
class ConfirmDialog : public wxDialog {
public:
    explicit ConfirmDialog(wxWindow* parent);
};
} // namespace ui_theme
