// Regression for B-BUG-008 and application-owned grey controls.
// Sends pointer/key events through real controls; does not claim hardware input.
#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/generic/treectlg.h>
#include <wx/popupwin.h>
#include <wx/filename.h>
#include <wx/modalhook.h>
#include <wx/uiaction.h>
#include <wx/propgrid/propgrid.h>
#include <wx/scrolwin.h>
#include "ui/property_panel.h"
#include <iostream>
#include <vector>
#include "ui/main_frame.h"
#include "ui/canvas_panel.h"
#include "ui/ui_theme.h"
#include "ui/subcircuit_library.h"
#include "model/schematic_model.h"

template<class T>T* Find(wxWindow* w){if(auto* p=dynamic_cast<T*>(w))return p;for(auto* c:w->GetChildren())if(auto* p=Find<T>(c))return p;return nullptr;}
ui_theme::ActionButton* Button(wxWindow* w,int id){if(auto* p=dynamic_cast<ui_theme::ActionButton*>(w);p&&p->GetId()==id)return p;for(auto* c:w->GetChildren())if(auto* p=Button(c,id))return p;return nullptr;}
ui_theme::ActionButton* LabelButton(wxWindow* w,const char* label){
    if(auto* p=dynamic_cast<ui_theme::ActionButton*>(w);p&&p->GetLabel()==wxString::FromUTF8(label))return p;
    for(auto* c:w->GetChildren())if(auto* p=LabelButton(c,label))return p;return nullptr;
}
wxWindow* Named(wxWindow* w,const wxString& name){if(w->GetName()==name)return w;for(auto* c:w->GetChildren())if(auto* p=Named(c,name))return p;return nullptr;}
wxWindow* Card(wxWindow* w,const char* title){return Named(w,"B-property-card-"+wxString::FromUTF8(title));}
wxStaticText* Value(wxWindow* w,const char* label){return dynamic_cast<wxStaticText*>(Named(w,"B-property-value-"+wxString::FromUTF8(label)));}
class GrayTest:public wxApp {
    UiMainFrame* frame=nullptr;wxGenericTreeCtrl* tree=nullptr;CanvasPanel* canvas=nullptr;
    wxTimer timer;int passed=0,failed=0,stage=0,commands=0;wxString temporary;
    int canvasWidth=0,toolbarId=0,propertiesId=0;
    void Check(const char* name,bool ok){(ok?++passed:++failed);std::cout<<(ok?"[PASS] ":"[FAIL] ")<<name<<std::endl;}
    void Mouse(wxWindow* w,wxEventType type,wxPoint p){wxMouseEvent e(type);e.SetEventObject(w);e.SetPosition(p);e.SetLeftDown(type==wxEVT_LEFT_DOWN||type==wxEVT_LEFT_DCLICK);w->ProcessWindowEvent(e);}
    void Click(wxWindow* w,wxPoint p,wxEventType down=wxEVT_LEFT_DOWN){Mouse(w,down,p);Mouse(w,wxEVT_LEFT_UP,p);}
    void Key(wxWindow* w,wxEventType type,int key){wxKeyEvent e(type);e.SetEventObject(w);e.m_keyCode=key;w->ProcessWindowEvent(e);}
    bool Pixel(wxWindow* w,wxPoint p,const wxColour& colour){wxYield();w->Refresh();w->Update();wxMilliSleep(25);wxYield();wxWindow* source=w;if(dynamic_cast<ui_theme::ActionButton*>(w)){p+=w->GetPosition();source=w->GetParent();}wxClientDC dc(source);wxColour result;const bool ok=dc.GetPixel(p.x,p.y,&result)&&result==colour;
        if(!ok)std::cout<<"PIXEL actual="<<int(result.Red())<<','<<int(result.Green())<<','<<int(result.Blue())<<" expected="<<int(colour.Red())<<','<<int(colour.Green())<<','<<int(colour.Blue())<<" size="<<w->GetSize().x<<'x'<<w->GetSize().y<<" shown="<<w->IsShownOnScreen()<<" point="<<p.x<<','<<p.y<<std::endl;
        return ok;}
    void Tick(){if(stage==0)Run();else FinishMenu();}
    void Run(){
        tree=Find<wxGenericTreeCtrl>(frame);canvas=Find<CanvasPanel>(frame);
        Check("palette exists",tree&&canvas);if(!tree||!canvas){Finish();return;}
        Check("default precision permits one-unit editing",canvas->SnapStep()==1);
        canvas->SetSnapStep(20); // The existing snapping cases below exercise the 20-unit option.
        std::vector<wxTreeItemId> items;wxTreeItemIdValue cookie;
        for(auto item=tree->GetFirstChild(tree->GetRootItem(),cookie);item.IsOk();item=tree->GetNextChild(tree->GetRootItem(),cookie))items.push_back(item);
        Check("five built-in types plus custom category",items.size()==6);
        bool uniform=true;int previousY=-1,rowHeight=-1;
        for(const auto& item:items){
            wxRect r;tree->GetBoundingRect(item,r);
            const auto font=tree->GetItemFont(item);
            uniform=uniform&&font.GetPixelSize()==tree->GetFont().GetPixelSize()&&font.GetWeight()==tree->GetFont().GetWeight();
            if(previousY>=0)uniform=uniform&&r.y-previousY==rowHeight;
            previousY=r.y;rowHeight=r.height;
        }
        Check("all palette labels share font, row height and spacing",uniform);
        auto* group=frame->FindWindow("B-command-toolbar");bool equalTools=group&&group->GetChildren().size()==6;
        if(group)for(auto* child:group->GetChildren())equalTools=equalTools&&dynamic_cast<ui_theme::ActionButton*>(child)&&child->GetSize()==wxSize(88,36);
        Check("six toolbar commands are equal-size text buttons on the right",equalTools&&group->GetScreenPosition().x>LabelButton(frame,"视图")->GetScreenPosition().x);
        Check("top navigation contains exactly File, Edit and View",frame->GetMenuBar()->GetMenuCount()==3&&LabelButton(frame,"文件")&&LabelButton(frame,"编辑")&&LabelButton(frame,"视图"));
        auto* card=Card(frame,"元件");
        Check("empty selection is a standalone white text card without a property grid",card&&!Find<wxPropertyGrid>(frame)&&Value(card,"选中")&&Value(card,"选中")->GetLabel()==wxString::FromUTF8("尚未选择元件")&&card->GetSize().y<Find<PropertyPanel>(frame)->GetSize().y/2);
        auto* viewMenu=frame->GetMenuBar()->GetMenu(2);
        toolbarId=viewMenu->FindItemByPosition(0)->GetId();propertiesId=viewMenu->FindItemByPosition(1)->GetId();
        const char* types[]={"AND","OR","NOT","SWITCH","LED"};int hitFailures=0,selectionFailures=0,clicks=0;
        for(int repeat=0;repeat<5;++repeat)for(int i=0;i<5;++i){
            wxRect r;tree->GetBoundingRect(items[i],r);
            for(int y:{r.y,r.y+1,r.y+r.height/2,r.y+r.height-1})for(int x:{1,30,r.width-2}){
                int flags;if(tree->HitTest({x,y},flags)!=items[i])++hitFailures;
                Click(tree,{x,y});if(canvas->PlacementType()!=types[i]||tree->GetSelection()!=items[i])++selectionFailures;++clicks;
            }
        }
        std::cout<<"ROW_CLICKS="<<clicks<<" hit_failures="<<hitFailures<<" selection_failures="<<selectionFailures<<std::endl;
        Check("all visible row pixels including top/bottom edges hit the intended item",hitFailures==0);
        Check("300 pointer transitions keep selected row and placement type in sync",selectionFailures==0);
        wxRect sw,led;tree->GetBoundingRect(items[3],sw);tree->GetBoundingRect(items[4],led);
        Click(tree,{30,sw.y});Click(tree,{30,led.y},wxEVT_LEFT_DCLICK);
        Check("adjacent double-click selects LED without an intermediate item",canvas->PlacementType()=="LED");
        if(wxGetenv("B_GRAY_NATIVE_POINTER")) {
            wxUIActionSimulator pointer;int misses=0;bool delivered=true;
            const auto nativeClick=[&](const wxRect& row){
                auto point=tree->ClientToScreen({30,row.y});

                delivered=pointer.MouseMove(point)&&pointer.MouseClick()&&delivered;
                wxMilliSleep(5);wxYield();
            };
            for(int i=0;i<25;++i){nativeClick(sw);if(canvas->PlacementType()!="SWITCH")++misses;nativeClick(led);if(canvas->PlacementType()!="LED")++misses;}
            std::cout<<"NATIVE_POINTER transitions=50 misses="<<misses<<std::endl;
            Check("display-server pointer transitions select adjacent rows without dead edges",delivered&&misses==0);
        }
        Mouse(tree,wxEVT_MOTION,{led.width-10,sw.y+sw.height/2});
        Check("selected palette row uses grey",Pixel(tree,{led.width-10,led.y+led.height/2},ui_theme::Selection()));
        Check("palette hover uses grey",Pixel(tree,{sw.width-10,sw.y+sw.height/2},ui_theme::Hover()));
        Mouse(tree,wxEVT_LEAVE_WINDOW,{-1,-1});
        tree->SelectItem(items[3]);Key(tree,wxEVT_CHAR,WXK_DOWN);
        Check("keyboard navigation still selects LED",canvas->PlacementType()=="LED");
        Click(canvas,{300,240});
        Check("palette selection places the intended LED in the document",frame->GetStatusBar()->GetStatusText(0).Contains("(LED)")&&frame->GetStatusBar()->GetStatusText(1).Contains(wxString::FromUTF8("元件 1")));
        canvas->OnDocumentReplaced();canvas->ClearHistory();wxYield();
        Check("document replacement leaves no stale selection after idle",!tree->GetSelection().IsOk()&&canvas->PlacementType().empty());
        tree->SelectItem(items[3]);tree->Collapse(items[5]);tree->Expand(items[5]);
        const auto child=tree->GetFirstChild(items[5],cookie);wxRect custom;tree->GetBoundingRect(child,custom);
        Click(tree,{30,custom.y});Check("custom fragment row uses the same edge hit path",canvas->HasSubcircuitPlacement());
        {
            ui_theme::NameDialog name(frame);
            name.CallAfter([&name]{auto* entry=Find<wxTextCtrl>(&name);entry->SetValue("keyboard-name");entry->SetFocus();wxKeyEvent e(wxEVT_CHAR_HOOK);e.m_keyCode=WXK_RETURN;name.ProcessWindowEvent(e);});
            Check("name dialog accepts Enter from its text field",name.ShowModal()==wxID_OK&&name.GetValue()=="keyboard-name");
            ui_theme::ConfirmDialog confirm(frame);
            confirm.CallAfter([&confirm]{auto* cancel=Button(&confirm,wxID_CANCEL);cancel->SetFocus();wxKeyEvent e(wxEVT_CHAR_HOOK);e.m_keyCode=WXK_RETURN;confirm.ProcessWindowEvent(e);});
            Check("overwrite dialog defaults Enter to Cancel",confirm.ShowModal()==wxID_CANCEL);
        }
        auto* fixture=new wxDialog(frame,wxID_ANY,"button fixture");
        auto* test=new ui_theme::ActionButton(fixture,wxID_HIGHEST+80,"test",frame);
        auto* fixtureLayout=new wxBoxSizer(wxVERTICAL);fixtureLayout->Add(test,0,wxALL,20);fixture->SetSizerAndFit(fixtureLayout);fixture->Show();
        frame->Bind(wxEVT_MENU,[this](wxCommandEvent&){++commands;},test->GetId());wxYield();
        Check("button default is near-white",Pixel(test,{12,18},ui_theme::Button()));
        if(wxGetenv("B_GRAY_NATIVE_POINTER")){wxUIActionSimulator pointer;pointer.MouseMove(test->ClientToScreen({12,18}));wxMilliSleep(25);wxYield();}
        Mouse(test,wxEVT_ENTER_WINDOW,{12,18});Check("button hover is grey",Pixel(test,{12,18},ui_theme::Hover()));
        Mouse(test,wxEVT_LEFT_DOWN,{12,18});Check("pressed button uses its own grey",Pixel(test,{12,18},ui_theme::Pressed()));
        Mouse(test,wxEVT_LEFT_UP,{12,18});Check("pointer activation dispatches the existing command once",commands==1);
        Mouse(test,wxEVT_LEFT_DOWN,{12,18});Mouse(test,wxEVT_LEFT_UP,{-2,-2});Check("releasing outside cancels activation",commands==1&&!test->HasCapture());
        Key(test,wxEVT_KEY_DOWN,WXK_SPACE);Key(test,wxEVT_KEY_DOWN,WXK_SPACE);Key(test,wxEVT_KEY_UP,WXK_SPACE);
        Check("held Space activates exactly once on release",commands==2);
        test->Enable(false);Click(test,{12,18});Key(test,wxEVT_KEY_DOWN,WXK_RETURN);Key(test,wxEVT_KEY_UP,WXK_RETURN);
        Check("disabled controls cannot dispatch commands",commands==2);
        Check("disabled button uses neutral grey",Pixel(test,{12,18},ui_theme::Disabled()));
        fixture->Destroy();wxYield();
        frame->UpdateWindowUI(wxUPDATE_UI_RECURSE);
        auto* undo=Button(frame,wxID_UNDO);Check("Undo button disables when history is empty",undo&&!undo->IsEnabled());
        Check("Save tool remains enabled",Button(frame,wxID_SAVE)&&Button(frame,wxID_SAVE)->IsEnabled());
        Check("native menu definitions retained and hidden",frame->GetMenuBar()&&!frame->GetMenuBar()->IsShown());
        auto* file=LabelButton(frame,"文件");
        Check("file menu trigger exists",file!=nullptr);if(!file){Finish();return;}
        file->Activate();stage=1;timer.StartOnce(300);
    }
    void FinishMenu(){
        auto* popup=Find<wxPopupTransientWindow>(frame);
        const auto openView=[this]{LabelButton(frame,"视图")->Activate();timer.StartOnce(180);};
        if(stage==1){
            Check("File opens the application-owned popup",popup!=nullptr);
            auto* next=popup?Button(popup,wxID_NEW):nullptr;
            Check("menu captions use mnemonic labels without visible Ctrl strings",next&&next->GetLabel()==wxString::FromUTF8("新建(N)")&&Button(popup,wxID_SAVEAS)->GetLabel()==wxString::FromUTF8("另存为(A)..."));
            if(!next){Finish();return;}next->Activate();stage=2;timer.StartOnce(180);return;
        }
        if(stage==2){
            Check("popup command reaches New document handler",frame->GetStatusBar()->GetStatusText(1).Contains(wxString::FromUTF8("元件 0"))&&canvas->PlacementType().empty());
            stage=3;openView();return;
        }
        if(stage==3){
            auto* tool=popup?Button(popup,toolbarId):nullptr;auto* properties=popup?Button(popup,propertiesId):nullptr;
            Check("View popup initially checks toolbar and properties",tool&&properties&&tool->IsChecked()&&properties->IsChecked());
            if(!tool){Finish();return;}tool->Activate();stage=4;timer.StartOnce(180);return;
        }
        if(stage==4){
            Check("hiding toolbar leaves three navigation entries and commands accessible",!frame->FindWindow("B-command-toolbar")->IsShown()&&LabelButton(frame,"视图")->IsShownOnScreen()&&frame->GetMenuBar()->FindItem(wxID_SAVE)->IsEnabled());
            if(wxGetenv("B_GRAY_NATIVE_POINTER")) {
                canvas->SetPlacementType("LED");Click(canvas,{200,200});canvas->ClearPlacementType();canvas->SetFocus();wxYield();
                wxUIActionSimulator keyboard;const bool delivered=keyboard.Char('N',wxMOD_CONTROL);wxMilliSleep(50);wxYield();
                Check("Ctrl+N remains active with hidden toolbar",delivered&&frame->GetStatusBar()->GetStatusText(1).Contains(wxString::FromUTF8("元件 0")));
            }
            stage=5;openView();return;
        }
        if(stage==5){
            auto* tool=popup?Button(popup,toolbarId):nullptr;
            Check("View checkbox reflects hidden toolbar",tool&&!tool->IsChecked());
            if(!tool){Finish();return;}tool->Activate();stage=6;timer.StartOnce(180);return;
        }
        if(stage==6){
            Check("toolbar restores six commands at original size",frame->FindWindow("B-command-toolbar")->IsShown()&&Button(frame,wxID_SAVE)->GetSize()==wxSize(88,36));
            canvasWidth=canvas->GetClientSize().x;stage=7;openView();return;
        }
        if(stage==7){
            auto* properties=popup?Button(popup,propertiesId):nullptr;
            if(!properties){Check("property toggle exists",false);Finish();return;}properties->Activate();stage=8;timer.StartOnce(180);return;
        }
        if(stage==8){
            Check("hiding properties reclaims canvas width",!Find<PropertyPanel>(frame)->IsShown()&&canvas->GetClientSize().x>canvasWidth+200);
            canvas->SetPlacementType("LED");Click(canvas,{200,200});canvas->ClearPlacementType();Click(canvas,{200,200});
            stage=9;openView();return;
        }
        if(stage==9){
            auto* properties=popup?Button(popup,propertiesId):nullptr;
            Check("View checkbox reflects hidden properties",properties&&!properties->IsChecked());
            if(!properties){Finish();return;}properties->Activate();stage=10;timer.StartOnce(180);return;
        }
        if(stage==10){
            auto* props=Find<PropertyPanel>(frame);auto* type=Value(frame,"类型");
            Check("properties restore original layout and current selection",props->IsShown()&&canvas->GetClientSize().x==canvasWidth&&type&&type->GetLabel()=="LED");
            canvas->ClearHistory();auto* location=Value(props,"位置");
            Mouse(canvas,wxEVT_LEFT_DOWN,{200,200});Mouse(canvas,wxEVT_MOTION,{247,233});
            Check("drag updates live snapped coordinates without rebuilding cards",location&&Value(props,"位置")==location&&location->GetLabel()=="(240, 240)"&&location->GetToolTipText()=="(240, 240)");
            Key(canvas,wxEVT_KEY_DOWN,WXK_ESCAPE);
            Check("cancelling drag restores displayed coordinates and keeps history empty",Value(props,"位置")==location&&location->GetLabel()=="(200, 200)"&&!canvas->CanUndo());
            Mouse(canvas,wxEVT_MIDDLE_DOWN,{300,300});Mouse(canvas,wxEVT_MOTION,{330,310});Mouse(canvas,wxEVT_MIDDLE_UP,{330,310});Mouse(canvas,wxEVT_LEFT_DOWN,{230,210});Mouse(canvas,wxEVT_MOTION,{277,243});
            Check("panned dragging still reports logical coordinates before release",Value(props,"位置")->GetLabel()=="(240, 240)");
            Mouse(canvas,wxEVT_LEFT_UP,{277,243});
            Click(canvas,{270,250});
            Check("reselecting released component retains coordinates and one reversible move",Value(props,"位置")->GetLabel()=="(240, 240)"&&canvas->CanUndo()&&canvas->Undo()&&!canvas->CanUndo());
            Click(canvas,{230,210});
            auto* component=Card(props,"元件");auto* geometry=Card(props,"几何");auto* pins=Card(props,"引脚");
            Check("component geometry and pins are separate white sibling cards",component&&geometry&&pins&&component->GetParent()==geometry->GetParent()&&pins->GetParent()==component->GetParent()&&component->GetRect().GetBottom()<geometry->GetPosition().y&&geometry->GetRect().GetBottom()<pins->GetPosition().y&&Pixel(component,{5,component->GetSize().y/2},ui_theme::Surface())&&Pixel(geometry,{5,geometry->GetSize().y/2},ui_theme::Surface())&&Pixel(pins,{5,pins->GetSize().y/2},ui_theme::Surface()));
            type=Value(props,"类型");
            Check("read-only information columns align across cards",type&&Value(geometry,"位置")&&Value(pins,"数量")&&type->GetPosition().x==Value(geometry,"位置")->GetPosition().x&&type->GetPosition().x==Value(pins,"数量")->GetPosition().x&&Value(pins,"[0] A"));
            props->ShowSelectionCount(4);wxYield();
            Check("multi-selection uses the same aligned card layout",Card(props,"多选")&&!Card(props,"元件")&&Value(props,"元件数量")&&Value(props,"元件数量")->GetLabel()=="4"&&Value(props,"封存"));
            editor::Component sample;sample.id="long-card";sample.type="AND";for(int i=0;i<16;++i)sample.name+="长名称";for(int i=0;i<24;++i)sample.pins.push_back({"P"+std::to_string(i),editor::PinDirection::Input,{-50,i*20}});
            props->ShowComponent(&sample);wxYield();auto* scroll=Find<wxScrolledWindow>(props);
            std::cout<<"CARD_SCROLL virtual="<<scroll->GetVirtualSize().x<<','<<scroll->GetVirtualSize().y<<" client="<<scroll->GetClientSize().x<<','<<scroll->GetClientSize().y<<" name="<<Value(props,"名称")->GetLabel()<<std::endl;
            Check("long information wraps and scrolls vertically without horizontal overflow",scroll&&scroll->GetVirtualSize().y>scroll->GetClientSize().y&&scroll->GetVirtualSize().x<=scroll->GetClientSize().x&&Value(props,"名称")->GetLabel().Contains("\n")&&Value(props,"[23] P23"));
            props->ShowComponent(nullptr);wxYield();
            Check("clearing selection removes stale geometry and pin cards",Card(props,"元件")&&!Card(props,"几何")&&!Card(props,"引脚")&&Value(props,"选中"));
            if(wxGetenv("B_GRAY_NATIVE_POINTER")) {
                canvas->SetPlacementType("SWITCH");Click(canvas,{430,330});canvas->ClearPlacementType();
                wxUIActionSimulator pointer;canvas->SetFocus();pointer.MouseMove(canvas->ClientToScreen({430,330}));wxMilliSleep(650);wxYield();
                bool pairs=true;
                for(int i=0;i<12;++i){pairs=pointer.MouseClick()&&pairs;wxMilliSleep(100);wxYield();if(i%2==1)pairs=pairs&&frame->GetStatusBar()->GetStatusText(0).Contains(wxString::FromUTF8(((i+1)/2)%2?"打开(1)":"关闭(0)"));}
                Check("main window switch toggles every native click pair with live property cards",pairs&&!canvas->HasCapture());
            }
        }
        Finish();
    }
    void Finish(){std::cout<<"SUMMARY "<<passed<<" passed, "<<failed<<" failed"<<std::endl;frame->Destroy();ExitMainLoop();}
public:
    bool OnInit()override{
        temporary=wxFileName::CreateTempFileName("eda-b-gray-");wxRemoveFile(temporary);
        editor::SchematicModel model;model.addElement("LED",{0,0});SubcircuitLibrary library(temporary);wxString error;library.Save(wxString::FromUTF8("测试子电路"),model.data(),false,&error);
        frame=new UiMainFrame(temporary);frame->Show();timer.Bind(wxEVT_TIMER,[this](wxTimerEvent&){Tick();});timer.StartOnce(650);return true;
    }
    int OnRun()override{wxApp::OnRun();return failed?1:0;}
    int OnExit()override{wxFileName::Rmdir(temporary,wxPATH_RMDIR_RECURSIVE);return 0;}
};wxIMPLEMENT_APP(GrayTest);
