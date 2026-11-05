// B: selection colour belongs to the component; an actual drag clears selection.
#include <wx/wx.h>
#include <iostream>
#include "ui/canvas_panel.h"
#include "model/schematic_model.h"
#include "simulation/simulator.h"
class SelectionTest:public wxApp {
    wxFrame* frame=nullptr;CanvasPanel* canvas=nullptr;editor::SchematicModel model;editor::Simulator sim;wxTimer timer;
    int passed=0,failed=0;size_t inspectedCount=0;std::string inspected;editor::Point inspectedPosition;
    void Check(const char* name,bool ok){(ok?++passed:++failed);std::cout<<(ok?"[PASS] ":"[FAIL] ")<<name<<std::endl;}
    void Mouse(wxEventType type,int x,int y){wxMouseEvent e(type);e.SetEventObject(canvas);e.SetPosition({x,y});e.SetLeftDown(type!=wxEVT_LEFT_UP);canvas->ProcessWindowEvent(e);}
    void Click(int x,int y){Mouse(wxEVT_LEFT_DOWN,x,y);Mouse(wxEVT_LEFT_UP,x,y);}
    void Key(int key){wxKeyEvent e(wxEVT_KEY_DOWN);e.m_keyCode=key;canvas->ProcessWindowEvent(e);}
    bool Position(const char* id,int x,int y){auto* c=model.findComponent(id);return c&&c->pos.x==x&&c->pos.y==y;}
    wxColour Pixel(int x,int y){canvas->Refresh();canvas->Update();wxYield();wxClientDC dc(canvas);wxColour pixel;dc.GetPixel(x,y,&pixel);return pixel;}
    void Reset(){model.loadFrom({});canvas->OnDocumentReplaced();canvas->ClearHistory();}
    void Box(int x,int y,int a,int b){Mouse(wxEVT_LEFT_DOWN,x,y);Mouse(wxEVT_MOTION,a,b);Mouse(wxEVT_LEFT_UP,a,b);}
    void Run(){
        Reset();model.addElement("AND",{300,240});canvas->OnDocumentReplaced();
        Mouse(wxEVT_MOTION,300,240);
        Check("hover has no blue frame outside the component",Pixel(265,244)==wxColour(255,255,255));
        Click(300,240);
        Check("click still selects the component",canvas->SelectedIds()==std::set<std::string>{"U1"});
        Check("selected component body has a blue fill",Pixel(300,240)==wxColour(222,236,255));
        Check("selected component outline is blue at its original boundary",Pixel(270,240)==wxColour(0,90,200));
        Check("selection adds no surrounding blue rectangle",Pixel(265,244)==wxColour(255,255,255));
        Mouse(wxEVT_LEFT_DOWN,300,240);Mouse(wxEVT_MOTION,302,242);
        Check("pointer jitter keeps ordinary click selection",canvas->SelectedIds().size()==1&&Position("U1",300,240));
        Mouse(wxEVT_MOTION,347,283);
        Check("actual drag immediately clears the selection",canvas->SelectedIds().empty()&&canvas->ActiveComponentCount()==1&&Position("U1",347,283));
        Check("dragged component is drawn in its normal colour",Pixel(347,283)==wxColour(255,255,255));
        Check("drag still reports the live component position",inspected=="U1"&&inspectedCount==1&&inspectedPosition.x==347&&inspectedPosition.y==283);
        Mouse(wxEVT_LEFT_UP,347,283);
        Check("release keeps the component deselected and clears inspection",canvas->SelectedIds().empty()&&canvas->ActiveComponentCount()==0&&inspected.empty());
        Key(WXK_RIGHT);
        Check("deselected component cannot be nudged or deleted",Position("U1",347,283)&&!canvas->DeleteSelection());
        Check("drag remains one undo and one redo",canvas->Undo()&&Position("U1",300,240)&&!canvas->CanUndo()&&canvas->Redo()&&Position("U1",347,283));
        Click(347,283);Mouse(wxEVT_LEFT_DOWN,347,283);Mouse(wxEVT_MOTION,377,303);Key(WXK_ESCAPE);
        Check("Escape restores both the original position and selection",Position("U1",347,283)&&canvas->SelectedIds()==std::set<std::string>{"U1"}&&Pixel(347,283)==wxColour(222,236,255));
        Check("cancelled drag does not create an extra history entry",canvas->Undo()&&Position("U1",300,240)&&!canvas->CanUndo());
        Reset();const auto a=model.addElement("AND",{300,240});const auto b=model.addElement("LED",{500,240});model.addWire({a,2},{b,0});canvas->OnDocumentReplaced();
        Box(250,200,550,280);Mouse(wxEVT_LEFT_DOWN,300,240);Mouse(wxEVT_MOTION,321,253);
        Check("deselection preserves the captured group for movement",canvas->SelectedIds().empty()&&canvas->ActiveComponentCount()==2&&Position("U1",321,253)&&Position("LED1",521,253)&&model.data().wires.size()==1);
        Check("group inspection count remains available during drag",inspectedCount==2);
        Mouse(wxEVT_LEFT_UP,321,253);
        Check("group release clears the whole selection",canvas->SelectedIds().empty()&&canvas->ActiveComponentCount()==0);
        Check("group drag still undoes all members in one action",canvas->Undo()&&Position("U1",300,240)&&Position("LED1",500,240)&&!canvas->CanUndo()&&canvas->Redo());
        Box(260,210,570,300);Mouse(wxEVT_LEFT_DOWN,321,253);Mouse(wxEVT_MOTION,345,280);
        if(canvas->HasCapture())canvas->ReleaseMouse();wxMouseCaptureLostEvent lost(canvas->GetId());lost.SetEventObject(canvas);canvas->ProcessWindowEvent(lost);
        Check("capture loss restores the captured positions and selected group",Position("U1",321,253)&&Position("LED1",521,253)&&canvas->SelectedIds().size()==2&&inspectedCount==2);
        Reset();const auto sw=model.addElement("SWITCH",{100,160});const auto led=model.addElement("LED",{300,160});model.addWire({sw,0},{led,0});canvas->OnDocumentReplaced();
        Click(100,160);Mouse(wxEVT_LEFT_DCLICK,100,160);Mouse(wxEVT_LEFT_UP,100,160);
        Check("selected switch body is blue without changing its High state",Pixel(115,175)==wxColour(222,236,255)&&sim.query("SW1",0)==editor::SignalLevel::High);
        Click(300,160);
        Check("selected LED retains its orange state indicator",Pixel(300,160)==wxColour(255,170,0));
        std::cout<<"SUMMARY "<<passed<<" passed, "<<failed<<" failed"<<std::endl;frame->Destroy();ExitMainLoop();
    }
public:
    bool OnInit()override{frame=new wxFrame(nullptr,wxID_ANY,"B selection",wxDefaultPosition,{900,650});canvas=new CanvasPanel(frame);auto* s=new wxBoxSizer(wxVERTICAL);s->Add(canvas,1,wxEXPAND);frame->SetSizer(s);canvas->Attach(&model,&sim);canvas->SetSelectionCallback([this](const editor::Component* c){inspectedCount=canvas->ActiveComponentCount();inspected=c?c->id:"";if(c)inspectedPosition=c->pos;});frame->Show();timer.Bind(wxEVT_TIMER,[this](wxTimerEvent&){Run();});timer.StartOnce(500);return true;}
    int OnRun()override{wxApp::OnRun();return failed?1:0;}
};wxIMPLEMENT_APP(SelectionTest);
