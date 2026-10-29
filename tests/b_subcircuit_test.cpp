// Integration test for B's selection -> save -> reload -> instantiate workflow.
// Compile with src/ui/*.cpp and the other implementation files, excluding main.cpp.
#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/modalhook.h>
#include <wx/treectrl.h>
#include <wx/textdlg.h>
#include <iostream>
#include <set>
#include "ui/canvas_panel.h"
#include "ui/main_frame.h"
#include "ui/subcircuit_library.h"
#include "model/schematic_model.h"
#include "simulation/simulator.h"
#include "io/netlist_io.h"

template<class T> T* FindChild(wxWindow* parent) {
    if (auto* value = dynamic_cast<T*>(parent)) return value;
    for (auto* child : parent->GetChildren()) if (auto* value = FindChild<T>(child)) return value;
    return nullptr;
}

class SaveDialogHook : public wxModalDialogHook {
public:
    wxString name = wxString::FromUTF8("菜单封存");
    int texts = 0, confirms = 0;
    bool overwrite = false;
    int Enter(wxDialog* dialog) override {
        if (auto* text = dynamic_cast<wxTextEntryDialog*>(dialog)) {
            ++texts; text->SetValue(name); return wxID_OK;
        }
        if (dynamic_cast<wxMessageDialog*>(dialog)) {
            ++confirms; return overwrite ? wxID_YES : wxID_NO;
        }
        return wxID_NONE;
    }
};

class SubcircuitTest : public wxApp {
public:
    bool OnInit() override {
        temporary = wxFileName::CreateTempFileName("2026eda-b-subcircuit-");
        wxRemoveFile(temporary);
        frame = new wxFrame(nullptr, wxID_ANY, "B subcircuit tests", wxDefaultPosition, wxSize(900,650));
        canvas = new CanvasPanel(frame); canvas->Attach(&model, &sim);
        auto* sizer = new wxBoxSizer(wxVERTICAL); sizer->Add(canvas,1,wxEXPAND); frame->SetSizer(sizer);
        canvas->SetStatusCallback([this](const wxString& s) { status=s; });
        frame->Show();
        timer.Bind(wxEVT_TIMER,[this](wxTimerEvent&) { Run(); }); timer.StartOnce(500);
        return true;
    }
    int OnRun() override { wxApp::OnRun(); return failed ? 1 : 0; }
    int OnExit() override { wxFileName::Rmdir(temporary,wxPATH_RMDIR_RECURSIVE); return 0; }
private:
    wxFrame* frame=nullptr;
    CanvasPanel* canvas=nullptr;
    editor::SchematicModel model;
    editor::Simulator sim;
    wxTimer timer;
    wxString temporary,status;
    int passed=0,failed=0;
    void Check(const char* name,bool ok) {
        ok ? ++passed : ++failed;
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    }
    void Mouse(wxEventType type,int x,int y,bool ctrl=false) {
        wxMouseEvent event(type); event.SetEventObject(canvas); event.SetPosition({x,y});
        event.SetLeftDown(type==wxEVT_LEFT_DOWN || type==wxEVT_MOTION); event.SetControlDown(ctrl);
        canvas->ProcessWindowEvent(event);
    }
    void Click(int x,int y,bool ctrl=false) { Mouse(wxEVT_LEFT_DOWN,x,y,ctrl); Mouse(wxEVT_LEFT_UP,x,y,ctrl); }
    void Esc() { wxKeyEvent e(wxEVT_KEY_DOWN); e.m_keyCode=WXK_ESCAPE; canvas->ProcessWindowEvent(e); }
    void Place(const char* type,int x,int y) { canvas->SetPlacementType(type); Click(x,y); Esc(); }
    void Wire(int x,int y,int a,int b) { Mouse(wxEVT_LEFT_DOWN,x,y); Mouse(wxEVT_MOTION,a,b); Mouse(wxEVT_LEFT_UP,a,b); }
    void Box(int x,int y,int a,int b) { Mouse(wxEVT_LEFT_DOWN,x,y); Mouse(wxEVT_MOTION,a,b); Mouse(wxEVT_LEFT_UP,a,b); }
    int SaveMenu(UiMainFrame* window) {
        auto* bar=window->GetMenuBar();
        for (size_t i=0;i<bar->GetMenuCount();++i)
            for (auto* item:bar->GetMenu(i)->GetMenuItems())
                if (item->GetItemLabelText()==wxString::FromUTF8("封存为子电路...")) return item->GetId();
        return wxID_NONE;
    }
    void Run() {
        SubcircuitLibrary library(temporary); wxString error;
        Check("missing library directory is harmless",library.Reload()==0 && library.Items().empty());
        Place("SWITCH",100,140); Place("AND",300,240); Place("LED",500,240);
        Wire(150,140,250,230); Wire(350,240,450,240);
        Check("source circuit has internal and boundary wires",model.data().components.size()==3 && model.data().wires.size()==2);
        Box(50,80,370,290);
        Check("box selects two full component bodies",canvas->SelectedIds()==std::set<std::string>({"SW1","U1"}));
        Click(500,240,true); Check("Ctrl adds a component",canvas->SelectedIds().size()==3);
        Click(500,240,true); Check("Ctrl removes a component",canvas->SelectedIds().size()==2);
        Mouse(wxEVT_LEFT_DOWN,20,20); Mouse(wxEVT_MOTION,30,30); Esc();
        Check("Esc restores selection before a cancelled box",canvas->SelectedIds().size()==2);

        editor::Schematic fragment;
        Check("export selection succeeds",canvas->ExportSelection(&fragment,&error));
        Check("only internal wire is retained",fragment.components.size()==2 && fragment.wires.size()==1 && fragment.nets.size()==1);
        Check("fragment origin matches model position bounds",fragment.components[0].pos.x==0 && fragment.components[0].pos.y==0 &&
              fragment.components[1].pos.x==200 && fragment.components[1].pos.y==100);
        Check("Unicode name saves ordinary Schematic JSON",library.Save(wxString::FromUTF8("子电路"),fragment,false,&error));
        Check("same name requires explicit overwrite",!library.Save(wxString::FromUTF8("子电路"),fragment,false,&error));
        Check("confirmed overwrite succeeds",library.Save(wxString::FromUTF8("子电路"),fragment,true,&error));
        for (const char* invalid: {"", "../outside", "folder/name", "CON", "bad.", "a:b"})
            Check("invalid filename is rejected",!library.Save(invalid,fragment,true,&error));
        Check("saving does not edit open document",model.data().components.size()==3 && model.data().wires.size()==2 &&
              model.findComponent("SW1")->pos.x==100);
        editor::NetlistIO io; editor::Schematic loaded;
        const auto file=wxFileName(temporary,wxString::FromUTF8("子电路.json")).GetFullPath();
        Check("round-trip file keeps normalized internal circuit",io.load(loaded,file.ToStdString(wxConvUTF8)) && loaded.components.size()==2 && loaded.wires.size()==1);
        auto invalid=fragment; invalid.components[1].id=invalid.components[0].id;
        Check("duplicate IDs cannot silently merge on placement",!SubcircuitLibrary::Validate(invalid,&error));
        invalid=fragment; invalid.wires[0].to.pinIndex=99;
        Check("invalid pin cannot silently lose its wire",!SubcircuitLibrary::Validate(invalid,&error));
        invalid=fragment; invalid.components[0].type="UNKNOWN";
        Check("unknown type is rejected",!SubcircuitLibrary::Validate(invalid,&error));
        invalid=fragment; invalid.components[0].pins[0].relPos.x=999;
        Check("incompatible pin geometry is rejected",!SubcircuitLibrary::Validate(invalid,&error));
        wxFile bad(wxFileName(temporary,"bad.json").GetFullPath(),wxFile::write); bad.Write("{bad"); bad.Close();
        Check("startup skips corrupt files and keeps valid ones",library.Reload()==1 && library.Items().size()==1);

        loaded.components[0].name="custom switch";

        Check("fragment enters placement mode",canvas->SetSubcircuitPlacement(loaded,"test"));
        Click(600,360);
        Check("instantiate creates one selected block with internal wire",model.data().components.size()==5 && model.data().wires.size()==3 && canvas->SelectedIds().size()==2);
        bool customName=false;
        for(const auto& id:canvas->SelectedIds()) if(model.findComponent(id)->name=="custom switch") customName=true;
        Check("instantiation retains explicit component names",customName);
        Check("one undo removes the entire block",canvas->Undo() && model.data().components.size()==3 && model.data().wires.size()==2);
        Check("one redo restores the entire block",canvas->Redo() && model.data().components.size()==5 && model.data().wires.size()==3);
        canvas->SetSubcircuitPlacement(loaded,"test"); Click(100,420); Esc();
        std::set<std::string> ids; for(const auto& c:model.data().components) ids.insert(c.id);
        Check("repeated placement assigns unique IDs",ids.size()==7 && model.data().components.size()==7 && model.data().wires.size()==4);
        const auto selected=canvas->SelectedIds();
        std::string sourceId; for(const auto& id:selected) if(model.findComponent(id)->type=="SWITCH") sourceId=id;
        Mouse(wxEVT_LEFT_DOWN,100,420); Mouse(wxEVT_MOTION,140,440); Mouse(wxEVT_LEFT_UP,140,440);
        Check("drag moves both selected components together",model.findComponent(sourceId)->pos.x==140 && model.findComponent(sourceId)->pos.y==440 && canvas->SelectedIds().size()==2);
        Check("group move is one undo",canvas->Undo() && model.findComponent(sourceId)->pos.x==100);
        Check("group move is one redo",canvas->Redo() && model.findComponent(sourceId)->pos.x==140);
        Box(60,390,380,570);
        Check("group can be reselected",canvas->SelectedIds().size()==2);
        Check("delete removes entire selection",canvas->DeleteSelection() && model.data().components.size()==5 && model.data().wires.size()==3);
        Check("group delete is one undo",canvas->Undo() && model.data().components.size()==7 && model.data().wires.size()==4);
        Box(60,390,380,570);
        Mouse(wxEVT_LEFT_DOWN,140,440); Mouse(wxEVT_MOTION,180,460); Esc();
        Check("Esc restores all members of a dragged group",model.findComponent(sourceId)->pos.x==140 && canvas->SelectedIds().size()==2);
        Check("cancelled group drag does not add history",canvas->Undo() && model.findComponent(sourceId)->pos.x==100);
        canvas->Redo();
        canvas->SetSubcircuitPlacement(loaded,"test"); Esc();
        Check("Esc clears subcircuit placement mode",!canvas->HasSubcircuitPlacement());
        Click(20,20); Check("empty selection cannot be saved",!canvas->ExportSelection(&invalid,&error));
        const auto count=model.data().components.size();
        Check("rejected fragment leaves model unchanged",!canvas->SetSubcircuitPlacement(invalid,"invalid") && model.data().components.size()==count);

        wxMouseEvent pan(wxEVT_MIDDLE_DOWN); pan.SetPosition({300,300}); pan.SetMiddleDown(true); canvas->ProcessWindowEvent(pan);
        Mouse(wxEVT_MOTION,340,320);
        wxMouseEvent panEnd(wxEVT_MIDDLE_UP); panEnd.SetPosition({340,320}); canvas->ProcessWindowEvent(panEnd);
        Box(410,310,90,100);
        Check("reverse box works after view panning",canvas->SelectedIds()==std::set<std::string>({"SW1","U1"}));
        const auto wiresBefore=model.data().wires.size();
        Click(190,160,true);
        Check("Ctrl click on a pin toggles selection without creating a wire",canvas->SelectedIds().size()==1 && model.data().wires.size()==wiresBefore);
        Click(540,260,true);
        Mouse(wxEVT_LEFT_DOWN,340,260); Mouse(wxEVT_MOTION,380,280);
        wxMouseCaptureLostEvent captureLost; canvas->ProcessWindowEvent(captureLost);
        Check("capture loss restores group positions",model.findComponent("U1")->pos.x==300 && model.findComponent("LED1")->pos.x==500);

        model.loadFrom(editor::Schematic{}); canvas->OnDocumentReplaced(); canvas->ClearHistory();
        for(int i=0;i<55;++i) Place("LED",100+(i%8)*80,100+(i/8)*60);
        for(int i=0;i<50;++i) canvas->Undo();
        Check("history limit stays at 50 edits",model.data().components.size()==5 && !canvas->CanUndo());
        for(int i=0;i<50;++i) canvas->Redo();
        Check("redo restores retained history without extra entries",model.data().components.size()==55 && !canvas->CanRedo());
        canvas->Undo(); Place("LED",740,500);
        Check("new edit clears redo branch",!canvas->CanRedo());

        auto* main=new UiMainFrame(temporary); main->Show(); wxYield();
        auto* original=canvas; canvas=FindChild<CanvasPanel>(main);
        auto* tree=FindChild<wxTreeCtrl>(main);
        wxTreeItemIdValue cookie; wxTreeItemId category;
        for(auto item=tree->GetFirstChild(tree->GetRootItem(),cookie);item.IsOk();item=tree->GetNextChild(tree->GetRootItem(),cookie))
            if(tree->GetItemText(item)==wxString::FromUTF8("自定义子电路")) category=item;
        Check("real palette creates custom category",category.IsOk());
        if(category.IsOk()) {
            const auto item=tree->GetFirstChild(category,cookie); tree->SelectItem(item); wxYield();
            Check("tree callback enables subcircuit placement",canvas->HasSubcircuitPlacement());
            Click(100,140); Check("palette placement selects the new block",canvas->SelectedIds().size()==2);
            SaveDialogHook hook; hook.Register();
            const int save=SaveMenu(main); Check("save subcircuit menu exists",save!=wxID_NONE);
            if(save!=wxID_NONE) {
                wxCommandEvent command(wxEVT_MENU,save); main->ProcessWindowEvent(command);
                Check("actual menu saves file and refreshes palette",hook.texts==1 && wxFileExists(wxFileName(temporary,hook.name+".json").GetFullPath()));
                main->ProcessWindowEvent(command); Check("overwrite can be cancelled",hook.confirms==1);
                hook.overwrite=true; main->ProcessWindowEvent(command); Check("overwrite can be confirmed",hook.confirms==2);
            }
            hook.Unregister();
        }
        main->Destroy(); canvas=original;
        std::cout << "SUMMARY " << passed << " passed, " << failed << " failed" << std::endl;
        frame->Destroy(); ExitMainLoop();
    }
};
wxIMPLEMENT_APP(SubcircuitTest);
