#include "ui/ui_theme.h"
#include <wx/dcbuffer.h>
#include <wx/popupwin.h>
#include <algorithm>
#include <memory>
#include <vector>

namespace ui_theme {
namespace { wxString U8(const char* text) { return wxString::FromUTF8(text); } }

wxGraphicsPath ContinuousRect(wxGraphicsContext& gc, const wxRect2DDouble& rect, double radius) {
    const double x=rect.m_x, y=rect.m_y, w=rect.m_width, h=rect.m_height;
    const double r=std::max(0.0,std::min({radius,w/2,h/2}));
    auto path=gc.CreatePath();
    path.MoveToPoint(x+r,y); path.AddLineToPoint(x+w-r,y);
    path.AddCurveToPoint(x+w,y,x+w,y,x+w,y+r);
    path.AddLineToPoint(x+w,y+h-r); path.AddCurveToPoint(x+w,y+h,x+w,y+h,x+w-r,y+h);
    path.AddLineToPoint(x+r,y+h); path.AddCurveToPoint(x,y+h,x,y+h,x,y+h-r);
    path.AddLineToPoint(x,y+r); path.AddCurveToPoint(x,y,x,y,x+r,y);
    path.CloseSubpath(); return path;
}
void DrawSurface(wxDC& dc, const wxRect& rect, const wxColour& fill, double radius, const wxColour& border) {
    std::unique_ptr<wxGraphicsContext> gc(wxGraphicsContext::CreateFromUnknownDC(dc));
    if (!gc || rect.width<=1 || rect.height<=1) return;
    gc->SetBrush(wxBrush(fill));
    gc->SetPen(border.IsOk()?wxPen(border,1):*wxTRANSPARENT_PEN);
    gc->DrawPath(ContinuousRect(*gc,wxRect2DDouble(rect.x+.5,rect.y+.5,rect.width-1,rect.height-1),radius));
}
wxPanel* Rule(wxWindow* parent) {
    auto* line=new wxPanel(parent,wxID_ANY);line->SetBackgroundColour(Border());line->SetMinSize(wxSize(-1,1));return line;
}
void StyleCard(wxPanel* panel) {
    panel->SetBackgroundStyle(wxBG_STYLE_PAINT);
    panel->SetBackgroundColour(Surface());panel->SetForegroundColour(Ink());panel->SetFont(Font());
    panel->Bind(wxEVT_PAINT,[panel](wxPaintEvent&){
        wxAutoBufferedPaintDC dc(panel);
        dc.SetBackground(wxBrush(panel->GetParent()->GetBackgroundColour()));dc.Clear();
        DrawSurface(dc,panel->GetClientRect(),Surface(),14,Border());
    });
}
wxPanel* SectionHeader(wxWindow* parent,const wxString& title,const wxString& subtitle,wxStaticText** captionControl) {
    auto* panel=new wxPanel(parent,wxID_ANY);panel->SetBackgroundColour(Surface());
    auto* layout=new wxBoxSizer(wxVERTICAL);
    auto* heading=new wxStaticText(panel,wxID_ANY,title);heading->SetFont(Font(20,true));heading->SetForegroundColour(Ink());
    auto* caption=new wxStaticText(panel,wxID_ANY,subtitle,wxDefaultPosition,wxDefaultSize,wxST_ELLIPSIZE_END);
    if(captionControl)*captionControl=caption;
    caption->SetMinSize(wxSize(0,-1));caption->SetFont(Font(16));caption->SetForegroundColour(Muted());
    layout->Add(heading,0,wxLEFT|wxRIGHT|wxTOP,8);layout->AddSpacer(4);
    layout->Add(caption,0,wxEXPAND|wxLEFT|wxRIGHT,8);layout->AddSpacer(10);panel->SetSizer(layout);return panel;
}
ActionButton::ActionButton(wxWindow* parent,int id,const wxString& label,wxWindow* target,const wxSize& size,wxEventType type)
    :wxControl(parent,id,wxDefaultPosition,size,wxBORDER_NONE|wxWANTS_CHARS),m_target(target),m_commandType(type) {
    SetLabel(label);SetName(label);SetMinSize(size);SetFont(Font(16));SetBackgroundStyle(wxBG_STYLE_PAINT);
    Bind(wxEVT_PAINT,&ActionButton::Paint,this);
    Bind(wxEVT_ENTER_WINDOW,[this](wxMouseEvent&){m_hover=true;Refresh();});
    Bind(wxEVT_LEAVE_WINDOW,[this](wxMouseEvent&){m_hover=false;Refresh();});
    Bind(wxEVT_LEFT_DOWN,[this](wxMouseEvent&){if(IsEnabled()){SetFocus();m_hover=true;m_pressed=true;CaptureMouse();Refresh();}});
    Bind(wxEVT_LEFT_DCLICK,[this](wxMouseEvent&){if(IsEnabled()&&!m_pressed){m_pressed=true;m_hover=true;CaptureMouse();Refresh();}});
    Bind(wxEVT_LEFT_UP,[this](wxMouseEvent& e){const bool fire=m_pressed&&IsEnabled()&&GetClientRect().Contains(e.GetPosition());ResetPress();if(fire)Activate();});
    Bind(wxEVT_MOUSE_CAPTURE_LOST,[this](wxMouseCaptureLostEvent&){m_pressed=false;m_keyPressed=false;Refresh();});
    Bind(wxEVT_SET_FOCUS,[this](wxFocusEvent& e){Refresh();e.Skip();});
    Bind(wxEVT_KILL_FOCUS,[this](wxFocusEvent& e){ResetPress();e.Skip();});
    Bind(wxEVT_KEY_DOWN,[this](wxKeyEvent& e){
        if(e.GetKeyCode()==WXK_ESCAPE){ResetPress();e.Skip();}
        else if(e.GetKeyCode()==WXK_SPACE||e.GetKeyCode()==WXK_RETURN){if(IsEnabled()){m_keyPressed=true;Refresh();}}
        else e.Skip();
    });
    Bind(wxEVT_KEY_UP,[this](wxKeyEvent& e){
        if((e.GetKeyCode()==WXK_SPACE||e.GetKeyCode()==WXK_RETURN)&&m_keyPressed){m_keyPressed=false;Refresh();Activate();}else e.Skip();
    });
    Bind(wxEVT_UPDATE_UI,[this](wxUpdateUIEvent& e){
        wxUpdateUIEvent query(GetId());query.SetEventObject(this);
        if(m_target->ProcessWindowEvent(query)&&query.GetSetEnabled())e.Enable(query.GetEnabled());else e.Skip();
    });
}
bool ActionButton::Enable(bool enable) {
    const bool changed=wxControl::Enable(enable);
    if(changed){if(!enable){m_hover=false;ResetPress();}else Refresh();}
    return changed;
}
void ActionButton::ResetPress(){m_pressed=false;m_keyPressed=false;if(HasCapture())ReleaseMouse();Refresh();}
void ActionButton::Activate(){if(!IsEnabled())return;wxCommandEvent command(m_commandType,GetId());command.SetEventObject(this);m_target->ProcessWindowEvent(command);}
void ActionButton::Paint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);dc.SetBackground(wxBrush(GetParent()->GetBackgroundColour()));dc.Clear();
    const auto fill=!IsEnabled()?Disabled():((m_pressed&&m_hover)||m_keyPressed)?Pressed():m_hover?Hover():Button();
    DrawSurface(dc,GetClientRect(),fill,8);
    if(HasFocus())DrawSurface(dc,GetClientRect().Deflate(3),fill,6,Focus());
    dc.SetFont(GetFont());dc.SetTextForeground(IsEnabled()?Ink():DisabledInk());
    const auto text=GetLabel();const auto extent=dc.GetTextExtent(text);const auto size=GetClientSize();
    dc.DrawText(text,(size.x-extent.x)/2,(size.y-extent.y)/2);
    // The tick occupies its own lane; labels stay centred in every menu row.
    if(m_checked) {
        dc.SetPen(wxPen(Ink(),1.6));const int y=size.y/2;
        dc.DrawLine(13,y,17,y+4);dc.DrawLine(17,y+4,24,y-4);
    }
}
namespace {
class CommandPopup : public wxPopupTransientWindow {
    wxWindow* m_target;
    std::vector<ActionButton*> m_buttons;
public:
    CommandPopup(wxWindow* target,wxMenu* menu):wxPopupTransientWindow(target,wxBORDER_NONE),m_target(target) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);SetBackgroundColour(Workspace());
        Bind(wxEVT_PAINT,[this](wxPaintEvent&){wxAutoBufferedPaintDC dc(this);dc.SetBackground(wxBrush(Workspace()));dc.Clear();DrawSurface(dc,GetClientRect(),Surface(),14,Border());});
        auto* layout=new wxBoxSizer(wxVERTICAL);layout->AddSpacer(8);
        for(auto* item:menu->GetMenuItems()) {
            if(item->IsSeparator()){layout->Add(Rule(this),0,wxEXPAND|wxALL,6);continue;}
            wxString text=item->GetItemLabel().BeforeFirst('\t');
            text.Replace("&", "");
            auto* button=new ActionButton(this,item->GetId(),text,this,wxSize(240,36),wxEVT_BUTTON);
            wxUpdateUIEvent update(item->GetId());update.SetEventObject(button);target->ProcessWindowEvent(update);
            button->Enable(update.GetSetEnabled()?update.GetEnabled():item->IsEnabled());
            if(item->IsCheckable())button->SetChecked(update.GetSetChecked()?update.GetChecked():item->IsChecked());
            m_buttons.push_back(button);layout->Add(button,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,8);
        }
        SetSizerAndFit(layout);
        Bind(wxEVT_BUTTON,[this](wxCommandEvent& e){
            const int id=e.GetId();auto* target=m_target;Dismiss();if(!IsBeingDeleted())Destroy();
            target->CallAfter([target,id]{wxCommandEvent command(wxEVT_MENU,id);target->ProcessWindowEvent(command);});
        });
        Bind(wxEVT_CHAR_HOOK,[this](wxKeyEvent& e){
            if(e.GetKeyCode()==WXK_ESCAPE){Dismiss();if(!IsBeingDeleted())Destroy();return;}
            if(e.GetKeyCode()!=WXK_UP&&e.GetKeyCode()!=WXK_DOWN){e.Skip();return;}
            int current=-1;for(size_t i=0;i<m_buttons.size();++i)if(m_buttons[i]->HasFocus())current=int(i);
            for(size_t n=0;n<m_buttons.size();++n){current=(current+(e.GetKeyCode()==WXK_UP?-1:1)+int(m_buttons.size()))%int(m_buttons.size());if(m_buttons[current]->IsEnabled()){m_buttons[current]->SetFocus();break;}}
        });
    }
    void OnDismiss()override {if(!IsBeingDeleted())Destroy();}
    wxWindow* FirstButton(){for(auto* b:m_buttons)if(b->IsEnabled())return b;return this;}
};
void DialogButtons(wxDialog* dialog,wxBoxSizer* layout,int accept,const wxString& label) {
    auto* row=new wxBoxSizer(wxHORIZONTAL);row->AddStretchSpacer();
    auto* cancel=new ActionButton(dialog,wxID_CANCEL,U8("取消"),dialog,wxSize(88,36),wxEVT_BUTTON);
    auto* ok=new ActionButton(dialog,accept,label,dialog,wxSize(88,36),wxEVT_BUTTON);
    row->Add(cancel,0,wxRIGHT,8);row->Add(ok);layout->Add(row,0,wxEXPAND|wxALL,18);
    dialog->SetEscapeId(wxID_CANCEL);dialog->SetAffirmativeId(accept);
    dialog->Bind(wxEVT_BUTTON,[dialog](wxCommandEvent& e){dialog->EndModal(e.GetId());});
    dialog->Bind(wxEVT_CHAR_HOOK,[cancel](wxKeyEvent& e){
        if(e.GetKeyCode()==WXK_ESCAPE)cancel->Activate();
        else if(e.GetKeyCode()==WXK_RETURN){
            if(auto* button=dynamic_cast<ActionButton*>(wxWindow::FindFocus()))button->Activate();
            else e.Skip();
        } else e.Skip();
    });
    cancel->SetFocus();
}
}
void ShowCommandMenu(wxWindow* target,wxMenu* menu,wxWindow* anchor){
    auto* popup=new CommandPopup(target,menu);popup->Position(anchor->GetScreenPosition(),wxSize(0,anchor->GetSize().y));popup->Popup(popup->FirstButton());
}
NameDialog::NameDialog(wxWindow* parent) {
    wxDialog::Create(parent,wxID_ANY,U8("封存为子电路"),wxDefaultPosition,wxDefaultSize,wxDEFAULT_DIALOG_STYLE);
    SetBackgroundColour(Surface());SetFont(Font());
    auto* layout=new wxBoxSizer(wxVERTICAL);
    auto* title=new wxStaticText(this,wxID_ANY,U8("输入子电路名称（自动添加 .json）"));title->SetForegroundColour(Ink());
    layout->Add(title,0,wxALL,18);m_textctrl=new wxTextCtrl(this,wxID_ANY);m_textctrl->SetMinSize(wxSize(360,-1));
    layout->Add(m_textctrl,0,wxEXPAND|wxLEFT|wxRIGHT,18);DialogButtons(this,layout,wxID_OK,U8("封存"));
    Bind(wxEVT_BUTTON,[this](wxCommandEvent&){m_value=m_textctrl->GetValue();EndModal(wxID_OK);},wxID_OK);
    Bind(wxEVT_CHAR_HOOK,[this](wxKeyEvent& e){
        if(e.GetKeyCode()==WXK_RETURN && m_textctrl->HasFocus()){m_value=m_textctrl->GetValue();EndModal(wxID_OK);}
        else e.Skip();
    });
    SetSizerAndFit(layout);CentreOnParent();m_textctrl->SetFocus();
}
ConfirmDialog::ConfirmDialog(wxWindow* parent):wxDialog(parent,wxID_ANY,U8("确认覆盖")) {
    SetBackgroundColour(Surface());SetFont(Font());auto* layout=new wxBoxSizer(wxVERTICAL);
    auto* text=new wxStaticText(this,wxID_ANY,U8("已有同名子电路，是否覆盖？"));text->SetForegroundColour(Ink());
    layout->Add(text,0,wxALL,18);DialogButtons(this,layout,wxID_YES,U8("覆盖"));SetSizerAndFit(layout);CentreOnParent();
}
} // namespace ui_theme
