// B's Linux rehearsal: real frame/menus/rendering, injected canvas events.
// Native file/name dialogs are answered by a test-only modal hook.
// This is an automated rehearsal, not a timed human presentation.
#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/filedlg.h>
#include <wx/modalhook.h>
#include <wx/treectrl.h>
#include <wx/generic/treectlg.h>
#include <wx/dcmemory.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include "ui/main_frame.h"
#include "ui/canvas_panel.h"
#include "ui/property_panel.h"
#include "ui/ui_theme.h"

namespace {
wxString U8(const char* s) { return wxString::FromUTF8(s); }
template<class T> T* Find(wxWindow* window) {
    if (auto* result = dynamic_cast<T*>(window)) return result;
    for (auto* child : window->GetChildren()) if (auto* result = Find<T>(child)) return result;
    return nullptr;
}
class Dialogs : public wxModalDialogHook {
public:
    wxString file, name = U8("演练子电路");
    int files = 0, names = 0;
    bool cancelFile = false;
    int Enter(wxDialog* dialog) override {
        if (auto* picker = dynamic_cast<wxFileDialog*>(dialog)) {
            ++files;
            if (cancelFile) return wxID_CANCEL;
            // wxGTK's chooser resolves its selected filename asynchronously,
            // after mapping. A SetPath-only hook would return an empty path.
            picker->Show();
            picker->SetPath(file);
            for (int attempt=0; attempt<200 && picker->GetPath()!=file; ++attempt) {
                wxMilliSleep(5); wxYield();
            }
            const bool ready = picker->GetPath()==file;
            picker->Hide();
            if (!ready) throw std::runtime_error("file chooser did not resolve path");
            return wxID_OK;
        }
        if (auto* input = dynamic_cast<wxTextEntryDialog*>(dialog)) {
            ++names; input->SetValue(name); return wxID_OK;
        }
        if (dynamic_cast<ui_theme::ConfirmDialog*>(dialog)) return wxID_YES;
        return wxID_NONE;
    }
};
}

class LinuxRehearsal : public wxApp {
public:
    bool OnInit() override {
        wxInitAllImageHandlers();
        output = wxGetenv("B_REHEARSAL_OUTPUT");
        if (output.empty()) {
            std::cerr << "Set B_REHEARSAL_OUTPUT to a new evidence directory.\n";
            return false;
        }
        // Refuse an existing directory: do not overwrite earlier evidence.
        if (wxDirExists(output) || wxFileExists(output) ||
            !wxFileName::Mkdir(output, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) return false;
        wxFileName absolute(output, ""); absolute.MakeAbsolute(); output = absolute.GetPath();
        phaseLog.open(Path("timings.tsv").ToStdString());
        phaseLog << "phase\telapsed_ms\n";
        hook.Register();
        frame = new UiMainFrame(Path("subcircuits"));
        frame->Show();
        canvas = Find<CanvasPanel>(frame);
        timer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { Run(); });
        timer.StartOnce(500);
        return true;
    }
    int OnRun() override { wxApp::OnRun(); return failed ? 1 : 0; }
    int OnExit() override { hook.Unregister(); return 0; }
private:
    UiMainFrame* frame = nullptr;
    CanvasPanel* canvas = nullptr;
    Dialogs hook;
    wxTimer timer;
    wxString output;
    std::ofstream phaseLog;
    int passed = 0, failed = 0;
    long timestamp = 10000;
    using Clock = std::chrono::steady_clock;
    Clock::time_point phaseStart;
    wxString Path(const wxString& name) const { return wxFileName(output, name).GetFullPath(); }
    void Check(const char* name, bool value) {
        std::cout << (value ? "[PASS] " : "[FAIL] ") << name << std::endl;
        value ? ++passed : ++failed;
        if (!value) throw std::runtime_error(name);
    }
    void Settle() { wxYield(); frame->Refresh(); frame->Update(); canvas->Update(); wxYield(); }
    void Begin() { phaseStart = Clock::now(); }
    void End(const char* name) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-phaseStart).count();
        phaseLog << name << '\t' << elapsed << '\n'; phaseLog.flush();
        std::cout << "PHASE " << name << " elapsed_ms=" << elapsed << std::endl;
    }
    int Menu(const wxString& prefix) {
        auto* bar = frame->GetMenuBar();
        for (size_t i=0; i<bar->GetMenuCount(); ++i)
            for (auto* item : bar->GetMenu(i)->GetMenuItems())
                if (item->GetItemLabelText().StartsWith(prefix)) return item->GetId();
        throw std::runtime_error("missing menu " + prefix.ToStdString());
    }
    void Command(int id, bool checked = false) {
        wxCommandEvent event(wxEVT_MENU, id); event.SetInt(checked);
        frame->ProcessWindowEvent(event); Settle();
    }
    void File(int id, const wxString& path) {
        wxFileName absolute(path); absolute.MakeAbsolute(); hook.file=absolute.GetFullPath(); Command(id);
    }
    bool Counts(int components, int wires, int nets) {
        std::cout << "STATUS " << frame->GetStatusBar()->GetStatusText(0).ToStdString(wxConvUTF8)
                  << " | " << frame->GetStatusBar()->GetStatusText(1).ToStdString(wxConvUTF8) << std::endl;
        return frame->GetStatusBar()->GetStatusText(1) ==
            wxString::Format(U8("元件 %d | 导线 %d | 网络 %d"), components,wires,nets);
    }
    void Mouse(wxEventType type, wxPoint at, bool ctrl=false) {
        wxMouseEvent event(type); event.SetPosition(at); event.SetEventObject(canvas);
        event.SetLeftDown(type==wxEVT_LEFT_DOWN || type==wxEVT_MOTION);
        event.SetControlDown(ctrl); event.SetTimestamp(timestamp++);
        canvas->ProcessWindowEvent(event);
    }
    void Click(wxPoint at, bool ctrl=false) { Mouse(wxEVT_LEFT_DOWN,at,ctrl); Mouse(wxEVT_LEFT_UP,at,ctrl); }
    void Key(int code) {
        wxKeyEvent event(wxEVT_KEY_DOWN); event.m_keyCode=code;
        canvas->ProcessWindowEvent(event); Settle();
    }
    void Toggle(wxPoint at) { timestamp+=2000; Click(at); Click(at); Settle(); }
    bool Led(wxPoint at, bool on) {
        Settle(); wxClientDC source(canvas); wxColour pixel;
        return source.GetPixel(at.x,at.y+3,&pixel) &&
            pixel == (on ? wxColour(255,170,0) : wxColour(255,255,255));
    }
    void Shot(const char* name) {
        Settle(); wxClientDC source(frame); const auto size=frame->GetClientSize();
        wxBitmap bitmap(size.x,size.y); wxMemoryDC target(bitmap);
        target.Blit(0,0,size.x,size.y,&source,0,0); target.SelectObject(wxNullBitmap);
        Check(name,bitmap.ConvertToImage().SaveFile(Path(U8(name)+".png"),wxBITMAP_TYPE_PNG));
    }
    editor::Schematic Saved() {
        File(wxID_SAVEAS,Path(U8("演练存档.json")));
        editor::NetlistIO io; editor::Schematic result;
        Check("saved JSON can be loaded",io.load(result,Path(U8("演练存档.json")).ToStdString(wxConvUTF8)));
        return result;
    }
    void Run() {
        try {
            std::cout << "MODE automated Linux rehearsal; file/name dialog hook; canvas event injection\n"
                      << "ENV content_scale=" << canvas->GetContentScaleFactor() << std::endl;
            const auto total=Clock::now();
            Begin();
            Check("main frame contains canvas and properties",canvas && Find<PropertyPanel>(frame));
            Check("default snapping is one unit",canvas->SnapStep()==1);
            File(wxID_OPEN,wxFileName("samples/demo.json").GetFullPath());
            Check("sample opens with four components three wires three nets",Counts(4,3,3));
            auto sample=Saved();
            Check("saved sample structure is intact",sample.components.size()==4 && sample.wires.size()==3 && sample.nets.size()==3);
            wxPoint sw1,sw2,led,gate;
            for(const auto& c:sample.components) {
                if(c.id=="SW1") sw1={c.pos.x,c.pos.y};
                if(c.id=="SW2") sw2={c.pos.x,c.pos.y};
                if(c.id=="LED1") led={c.pos.x,c.pos.y};
                if(c.id=="U1") gate={c.pos.x,c.pos.y};
            }
            End("01-open-save");
            Begin();
            Check("AND 00 gives LED off",Led(led,false));
            Toggle(sw1); Check("AND 10 gives LED off",Led(led,false));
            Toggle(sw2); Check("AND 11 gives LED on",Led(led,true));
            Toggle(sw1); Check("AND 01 gives LED off",Led(led,false));
            Toggle(sw2); Check("AND returns to 00",Led(led,false));
            End("02-truth-table");
            Begin();
            Click(gate); Settle(); Check("single selection",canvas->SelectedIds().count("U1")==1);
            Mouse(wxEVT_LEFT_DOWN,gate); Mouse(wxEVT_MOTION,gate+wxPoint(7,3)); Settle();
            auto* live=frame->FindWindow(U8("B-property-value-位置"));
            Check("live geometry reflects precise drag",live && live->GetLabel()==wxString::Format("(%d, %d)",gate.x+7,gate.y+3));
            Check("drag clears selected IDs",canvas->SelectedIds().empty());
            Mouse(wxEVT_LEFT_UP,gate+wxPoint(7,3)); Settle();
            Check("released drag remains deselected",canvas->SelectedIds().empty());
            Command(wxID_UNDO); Click(gate); Key(WXK_RIGHT);
            Check("one-unit nudge records history",canvas->CanUndo()); Command(wxID_UNDO);
            Command(wxID_REDO); Command(wxID_UNDO);
            Command(Menu(U8("吸附 5")),true); Check("five-unit menu setting",canvas->SnapStep()==5);
            Command(Menu(U8("吸附 1 单位")),true); Check("one-unit menu restored",canvas->SnapStep()==1);
            End("03-drag-history-precision");
            Begin();
            Click(sw1); Click(sw2,true); Click(gate,true); Click(led,true);
            Check("select all four components",canvas->SelectedIds().size()==4);
            Command(Menu(U8("封存为子电路")));
            const auto fragmentFile=wxFileName(Path("subcircuits"),hook.name+".json").GetFullPath();
            editor::Schematic fragment; editor::NetlistIO io;
            Check("menu saves named subcircuit",hook.names==1 && io.load(fragment,fragmentFile.ToStdString(wxConvUTF8)));
            Check("fragment keeps three internal wires",fragment.components.size()==4 && fragment.wires.size()==3 && fragment.nets.size()==3);
            Command(wxID_NEW); Check("new document clears contents",Counts(0,0,0));
            auto* tree=Find<wxGenericTreeCtrl>(frame); wxTreeItemId found;
            if(tree) {
                wxTreeItemIdValue cookie; auto item=tree->GetFirstChild(tree->GetRootItem(),cookie);
                while(item.IsOk()) {
                    if(tree->GetItemText(item)==U8("自定义子电路")) {
                        tree->Expand(item); wxTreeItemIdValue nested; found=tree->GetFirstChild(item,nested); break;
                    }
                    item=tree->GetNextChild(tree->GetRootItem(),cookie);
                }
            }
            Check("saved fragment appears in palette",found.IsOk()); tree->SelectItem(found); Settle();
            Check("palette enters fragment placement",canvas->HasSubcircuitPlacement());
            Click({100,140}); Key(WXK_ESCAPE); Check("fragment restores circuit structure",Counts(4,3,3));
            auto before=Saved(); const wxPoint first(before.components.front().pos.x,before.components.front().pos.y);
            Mouse(wxEVT_LEFT_DOWN,first); Mouse(wxEVT_MOTION,first+wxPoint(7,3)); Mouse(wxEVT_LEFT_UP,first+wxPoint(7,3)); Settle();
            Command(wxID_UNDO); Command(wxID_UNDO); Check("group move and group placement undo separately",Counts(0,0,0));
            End("04-subcircuit");
            Begin();
            File(wxID_OPEN,wxFileName("samples/demo.json").GetFullPath());
            const auto exportFile=Path("demo.net"); File(Menu(U8("导出网表")),exportFile);
            Check("export menu creates net and pcb together",wxFileExists(exportFile) && wxFileExists(Path("demo.pcb")));
            Check("export success names both files",frame->GetStatusBar()->GetStatusText(0).Contains("demo.net") && frame->GetStatusBar()->GetStatusText(0).Contains("demo.pcb"));
            hook.cancelFile=true; Command(wxID_OPEN); hook.cancelFile=false;
            Check("cancelled open preserves circuit",Counts(4,3,3));
            { std::ofstream invalid(Path("invalid.json").ToStdString()); invalid<<"{broken"; }
            File(wxID_OPEN,Path("invalid.json")); Check("invalid open preserves circuit",Counts(4,3,3));
            End("05-files-export");
            Begin();
            auto* properties=Find<PropertyPanel>(frame); const int width=canvas->GetClientSize().x;
            Command(Menu(U8("元件属性")),false); Check("hide properties expands canvas",!properties->IsShown() && canvas->GetClientSize().x>width);
            Command(Menu(U8("元件属性")),true); Check("restore properties restores width",properties->IsShown() && canvas->GetClientSize().x==width);
            Command(Menu(U8("工具栏")),false); Check("toolbar can be hidden",!frame->FindWindow("B-command-toolbar")->IsShown());
            Command(Menu(U8("工具栏")),true);
            frame->SetSize(1000,680); Settle(); Check("minimum window keeps usable canvas",canvas->GetClientSize().x>0 && canvas->GetClientSize().y>0);
            Shot("minimum"); frame->SetSize(1100,800); Settle();
            Click(gate); Shot("final"); End("06-view-layout");
            const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now()-total).count();
            std::cout<<"TOTAL automated_elapsed_ms="<<elapsed<<" (excludes gEDA import and human presentation)"<<std::endl;
        } catch(const std::exception& error) {
            if(!failed)++failed;
            std::cerr<<"REHEARSAL_STOP "<<error.what()<<std::endl;
        }
        std::cout<<"SUMMARY "<<passed<<" passed, "<<failed<<" failed"<<std::endl;
        phaseLog.close(); frame->Destroy(); ExitMainLoop();
    }
};
wxIMPLEMENT_APP(LinuxRehearsal);
