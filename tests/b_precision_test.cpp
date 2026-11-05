// B: fine placement, drag, Alt bypass, keyboard nudging and View precision choices.
#include <wx/wx.h>
#include <wx/popupwin.h>
#include <iostream>
#include "ui/canvas_panel.h"
#include "ui/main_frame.h"
#include "ui/ui_theme.h"
#include "model/schematic_model.h"
#include "simulation/simulator.h"

template<class T>T* Find(wxWindow* w){if(auto* p=dynamic_cast<T*>(w))return p;for(auto* c:w->GetChildren())if(auto* p=Find<T>(c))return p;return nullptr;}
ui_theme::ActionButton* Button(wxWindow* w,int id){if(auto* p=dynamic_cast<ui_theme::ActionButton*>(w);p&&p->GetId()==id)return p;for(auto* c:w->GetChildren())if(auto* p=Button(c,id))return p;return nullptr;}
class PrecisionTest:public wxApp {
    wxFrame* host=nullptr;CanvasPanel* canvas=nullptr;editor::SchematicModel model;editor::Simulator sim;wxTimer timer;int passed=0,failed=0;
    void Check(const char* name,bool ok){(ok?++passed:++failed);std::cout<<(ok?"[PASS] ":"[FAIL] ")<<name<<std::endl;}
    void Mouse(wxEventType type,int x,int y,bool alt=false){wxMouseEvent e(type);e.SetEventObject(canvas);e.SetPosition({x,y});e.SetLeftDown(type!=wxEVT_LEFT_UP);e.SetAltDown(alt);canvas->ProcessWindowEvent(e);}
    void Click(int x,int y,bool alt=false){Mouse(wxEVT_LEFT_DOWN,x,y,alt);Mouse(wxEVT_LEFT_UP,x,y,alt);}
    void Key(int key,bool shift=false){wxKeyEvent e(wxEVT_KEY_DOWN);e.m_keyCode=key;e.SetShiftDown(shift);canvas->ProcessWindowEvent(e);}
    void Reset(){model.loadFrom({});canvas->OnDocumentReplaced();canvas->ClearHistory();}
    bool Position(const std::string& id,int x,int y){auto* c=model.findComponent(id);return c&&c->pos.x==x&&c->pos.y==y;}
    void Place(int x,int y,bool alt=false){canvas->SetPlacementType("AND");Click(x,y,alt);canvas->ClearPlacementType();}
    void Run(){
        Check("new documents default to one-unit precision",canvas->SnapStep()==1);
        Place(137,163);Check("placement can stop between ten-unit guide lines",Position("U1",137,163));
        canvas->ClearHistory();Mouse(wxEVT_LEFT_DOWN,140,166);Mouse(wxEVT_MOTION,184,218);
        Check("fine dragging preserves grab offset and exact live coordinates",Position("U1",181,215));
        Mouse(wxEVT_LEFT_UP,185,219);Check("release keeps one-unit precision",Position("U1",182,216));
        Check("fine movement remains a single undo",canvas->Undo()&&Position("U1",137,163)&&!canvas->CanUndo());
        Check("redo restores the exact off-grid position",canvas->Redo()&&Position("U1",182,216));
        const int steps[]={1,5,10,20},xs[]={153,155,150,160},ys[]={167,165,170,160};bool choices=true;
        for(int i=0;i<4;++i){Reset();choices=canvas->SetSnapStep(steps[i])&&choices;Place(153,167);choices=Position("U1",xs[i],ys[i])&&choices;}
        Check("1 5 10 and 20 unit placement options use their selected precision",choices);
        bool invalid=true;for(int step:{0,-1,3,50})invalid=!canvas->SetSnapStep(step)&&invalid;
        Check("invalid precision cannot change the current option",invalid&&canvas->SnapStep()==20);
        Reset();Place(153,167,true);Check("Alt placement bypasses coarse snapping",Position("U1",153,167));
        canvas->ClearHistory();Mouse(wxEVT_LEFT_DOWN,156,169);Mouse(wxEVT_MOTION,231,243,true);Mouse(wxEVT_LEFT_UP,231,243,true);
        Check("Alt drag and release retain exact one-unit coordinates",Position("U1",228,241)&&canvas->SnapStep()==20);
        Click(228,241);Key(WXK_RIGHT);Check("arrow key nudges one unit even in coarse mode",Position("U1",229,241));
        Key(WXK_UP,true);Check("Shift arrow nudges ten units",Position("U1",229,231));
        Check("keyboard nudge is reversible as one action",canvas->Undo()&&Position("U1",229,241));
        Mouse(wxEVT_LEFT_DOWN,229,241);Mouse(wxEVT_MOTION,277,279,true);Key(WXK_ESCAPE);
        Check("cancelling a fine drag restores the exact original position",Position("U1",229,241));
        Reset();editor::SchematicModel fragment;const auto a=fragment.addElement("AND",{0,0});const auto b=fragment.addElement("LED",{200,0});fragment.addWire({a,2},{b,0});
        Check("fine placement accepts an ordinary subcircuit",canvas->SetSubcircuitPlacement(fragment.data(),"fine"));Click(333,317,true);canvas->ClearPlacementType();
        auto ids=canvas->SelectedIds();bool block=ids.size()==2;int first=0;for(const auto& id:ids){auto* c=model.findComponent(id);block=block&&c&&c->pos.y==317;if(c)first+=c->pos.x;}
        Check("Alt subcircuit placement retains internal offsets and wire",block&&first==866&&model.data().wires.size()==1);
        canvas->ClearHistory();Key(WXK_RIGHT);int nudged=0;for(const auto& id:ids)nudged+=model.findComponent(id)->pos.x;
        Check("one-unit group nudging preserves relative positions",nudged==868&&canvas->Undo()&&!canvas->CanUndo());
        // Exercise the actual View command wiring, including exclusive check marks.
        auto* main=new UiMainFrame("/tmp/eda-precision-empty-library");main->Show();wxYield();auto* actual=Find<CanvasPanel>(main);auto* menu=main->GetMenuBar()->GetMenu(2);
        bool radios=true;for(int i=0;i<4;++i){auto* item=menu->FindItemByPosition(3+i);wxCommandEvent e(wxEVT_MENU,item->GetId());main->ProcessWindowEvent(e);radios=radios&&actual->SnapStep()==steps[i]&&item->IsChecked();for(int j=0;j<4;++j)radios=radios&&(menu->FindItemByPosition(3+j)->IsChecked()==(i==j));}
        Check("View precision choices update snapping with one exclusive selection",radios);
        auto* item=menu->FindItemByPosition(3);auto* top=main->FindWindow("B-top-bar");auto it=top->GetChildren().begin();++it;++it;auto* view=dynamic_cast<ui_theme::ActionButton*>(*it);view->Activate();wxYield();auto* popup=Find<wxPopupTransientWindow>(main);auto* choice=popup?Button(popup,item->GetId()):nullptr;
        Check("precision is available from the visible View popup",choice&&Button(popup,menu->FindItemByPosition(6)->GetId())->IsChecked());
        if(choice){choice->Activate();wxYield();}
        Check("popup selection switches back to one-unit mode",actual->SnapStep()==1);
        main->Destroy();std::cout<<"SUMMARY "<<passed<<" passed, "<<failed<<" failed"<<std::endl;host->Destroy();ExitMainLoop();
    }
public:
    bool OnInit()override{host=new wxFrame(nullptr,wxID_ANY,"B precision",wxDefaultPosition,{900,650});canvas=new CanvasPanel(host);auto* s=new wxBoxSizer(wxVERTICAL);s->Add(canvas,1,wxEXPAND);host->SetSizer(s);canvas->Attach(&model,&sim);host->Show();timer.Bind(wxEVT_TIMER,[this](wxTimerEvent&){Run();});timer.StartOnce(500);return true;}
    int OnRun()override{wxApp::OnRun();return failed?1:0;}
};wxIMPLEMENT_APP(PrecisionTest);
