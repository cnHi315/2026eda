// B's GUI bug audit. Uses public UI events and model/simulator queries only.
// Build separately; no change to the shared CMakeLists.txt is required.
#include <wx/wx.h>
#include <wx/modalhook.h>
#include <wx/treectrl.h>
#include <iostream>
#include <string>

#include "components/component_library.h"
#include "model/schematic_model.h"
#include "simulation/simulator.h"
#include "ui/canvas_panel.h"
#include "ui/main_frame.h"

template<class T> T* Child(wxWindow* parent) {
    if (auto* found = dynamic_cast<T*>(parent)) return found;
    for (auto* w : parent->GetChildren()) if (auto* found = Child<T>(w)) return found;
    return nullptr;
}

class FileDialogProbe : public wxModalDialogHook {
public:
    int calls = 0;
    wxString wildcard;
    int Enter(wxDialog* dialog) override {
        if (auto* file = dynamic_cast<wxFileDialog*>(dialog)) {
            ++calls;
            wildcard = file->GetWildcard();
            // Inspect the actual menu-created object, then simulate cancellation.
            // This deliberately does not claim to click the native file chooser.
            return wxID_CANCEL;
        }
        return wxID_NONE;
    }
};

class BugAudit : public wxApp {
public:
    bool OnInit() override {
        frame = new wxFrame(nullptr, wxID_ANY, "B bug audit", wxDefaultPosition, wxSize(900, 650));
        canvas = new CanvasPanel(frame);
        auto* layout = new wxBoxSizer(wxVERTICAL);
        layout->Add(canvas, 1, wxEXPAND);
        frame->SetSizer(layout);
        frame->Layout();
        canvas->Attach(&model, &sim);
        canvas->SetStatusCallback([this](const wxString& s) { status = s; });
        canvas->Bind(wxEVT_PAINT, [this](wxPaintEvent& e) { ++paints; e.Skip(); });
        frame->Show();
        wxInitAllImageHandlers();
        timer.Bind(wxEVT_TIMER, [this](wxTimerEvent&) { Run(); });
        timer.StartOnce(500); // Wait for GTK mapping/layout before pixel probes.
        return true;
    }
    int OnRun() override { wxApp::OnRun(); return failures ? 1 : (knownBugs ? 2 : 0); }

private:
    wxFrame* frame = nullptr;
    CanvasPanel* canvas = nullptr;
    editor::SchematicModel model;
    editor::Simulator sim;
    wxString status;
    int paints = 0, passed = 0, failures = 0, knownBugs = 0;
    wxTimer timer;

    void Check(const char* name, bool ok) {
        (ok ? ++passed : ++failures);
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    }
    void Preserved(const char* name, bool ok) {
        if (ok) ++passed; else ++knownBugs;
        std::cout << (ok ? "[PASS] " : "[BUG] ") << name << std::endl;
    }
    void Mouse(wxEventType type, int x, int y) {
        wxMouseEvent event(type);
        event.SetEventObject(canvas);
        event.SetPosition(wxPoint(x, y));
        event.SetLeftDown(type == wxEVT_LEFT_DOWN || type == wxEVT_MOTION);
        canvas->ProcessWindowEvent(event);
    }
    void Key(int key) {
        wxKeyEvent event(wxEVT_KEY_DOWN);
        event.m_keyCode = key;
        canvas->ProcessWindowEvent(event);
    }
    void Click(int x, int y) {
        Mouse(wxEVT_LEFT_DOWN, x, y); Mouse(wxEVT_LEFT_UP, x, y);
    }
    void Toggle(int x, int y) {
        Click(x, y); Mouse(wxEVT_LEFT_DCLICK, x, y);
    }
    void Reset() {
        model.loadFrom(editor::Schematic{});
        canvas->OnDocumentReplaced(); canvas->ClearHistory();
        // A faraway element removes the empty-document hint from pixel baselines.
        model.addElement("LED", {700, 450});
        canvas->OnDocumentReplaced();
        wxYield(); canvas->Update();
    }
    void Place(const char* type, int x, int y) {
        canvas->SetPlacementType(type); Click(x, y); Key(WXK_ESCAPE);
        canvas->Update();
    }
    wxImage Snapshot(const char* name) {
        wxClientDC source(canvas);
        const wxSize size = canvas->GetClientSize();
        wxBitmap bitmap(size.x, size.y);
        wxMemoryDC target(bitmap);
        target.Blit(0, 0, size.x, size.y, &source, 0, 0);
        target.SelectObject(wxNullBitmap);
        wxImage result = bitmap.ConvertToImage();
        const wxString path = wxGetenv("B_AUDIT_EVIDENCE");
        if (!path.empty()) {
            result.SaveFile(path + "/" + name + ".png", wxBITMAP_TYPE_PNG);
        }
        return result;
    }
    bool SameRegion(const wxImage& a, const wxImage& b, wxRect r) {
        for (int y = r.y; y < r.GetBottom(); ++y)
            for (int x = r.x; x < r.GetRight(); ++x)
                if (a.GetRed(x,y) != b.GetRed(x,y) ||
                    a.GetGreen(x,y) != b.GetGreen(x,y) || a.GetBlue(x,y) != b.GetBlue(x,y)) return false;
        return true;
    }
    void Run() {
        std::cout << "ENV content_scale=" << canvas->GetContentScaleFactor()
                  << " dpi_scale=" << canvas->GetDPIScaleFactor()
                  << " canvas=" << canvas->GetClientSize().x << 'x' << canvas->GetClientSize().y << std::endl;
        Reset();
        wxClientDC grid(canvas);
        wxColour major, minor, background;
        const bool pixels = grid.GetPixel(100, 17, &major) && grid.GetPixel(120, 17, &minor)
                         && grid.GetPixel(121, 17, &background);
        std::cout << "GRID major=" << int(major.Red()) << ',' << int(major.Green()) << ',' << int(major.Blue())
                  << " minor=" << int(minor.Red()) << ',' << int(minor.Green()) << ',' << int(minor.Blue())
                  << " background=" << int(background.Red()) << ',' << int(background.Green()) << ',' << int(background.Blue()) << std::endl;
        Check("B-BUG-003 grid stays at 20 logical units without double scaling",
              pixels && major.Red() == 215 && minor.Red() == 235 && background.Red() == 255);

        editor::ComponentLibrary library;
        for (const auto& type : library.types()) {
            Reset(); Place(type.c_str(), 300, 240);
            bool aligned = true;
            for (const auto& pin : library.pinTemplate(type)) {
                wxClientDC dc(canvas); wxColour dot;
                const int x = 300 + pin.relPos.x, y = 240 + pin.relPos.y;
                aligned = aligned && dc.GetPixel(x, y, &dot) && dot.Red() < 80 && dot.Green() < 80 && dot.Blue() < 80;
                Mouse(wxEVT_LEFT_DOWN, x, y);
                aligned = aligned && status.Contains(wxString::FromUTF8("连线中"));
                Key(WXK_ESCAPE); Mouse(wxEVT_LEFT_UP, x, y); canvas->Update();
            }
            const auto name = "B-BUG-004 rendered pin and hit target align: " + type;
            Check(name.c_str(), aligned);
        }

        Reset();
        const auto gridOnly = Snapshot("label-region-baseline");
        Place("AND", 300, 240);
        Mouse(wxEVT_LEFT_DOWN, 300, 240);
        Mouse(wxEVT_MOTION, 160, 240); Mouse(wxEVT_LEFT_UP, 160, 240);
        // No unconditional Refresh(): exercise the production dirty-region path.
        canvas->Update();
        const auto moved = Snapshot("after-left-drag");
        Check("B-HIS-001 left drag clears the old Y label region", SameRegion(gridOnly, moved, wxRect(350, 210, 36, 32)));
        const int beforeUndoPaints = paints;
        canvas->Undo();
        Check("B-HIS-002 undo paints before returning without injected Refresh", paints > beforeUndoPaints);
        const auto* gate = model.findComponent("U1");
        Check("B-HIS-003 undo restores drag position", gate && gate->pos.x == 300 && gate->pos.y == 240);
        Mouse(wxEVT_LEFT_DOWN, 300, 240); Mouse(wxEVT_MOTION, 360, 280);
        const auto before = model.data().components.size();
        canvas->Undo();
        gate = model.findComponent("U1");
        Check("B-HIS-004 undo during drag leaves previous history intact", gate && gate->pos.x == 360 &&
              model.data().components.size() == before && status.Contains(wxString::FromUTF8("正在拖动")));
        Key(WXK_ESCAPE); Mouse(wxEVT_LEFT_UP, 360, 280);

        Reset();
        const auto labelBaseline = Snapshot("long-label-baseline");
        model.addElement("AND", {300,240});
        auto document = model.data();
        const std::string longName(20, 'W');
        document.components.back().name = longName;
        model.loadFrom(document); canvas->OnDocumentReplaced();
        Mouse(wxEVT_LEFT_DOWN,300,240); Mouse(wxEVT_MOTION,100,240); Mouse(wxEVT_LEFT_UP,100,240);
        canvas->Update();
        const auto longMoved = Snapshot("long-label-after-drag");
        canvas->Refresh(); canvas->Update();
        const auto fullRedraw = Snapshot("long-label-full-redraw");
        Check("B-BUG-007 long-label dirty redraw matches full redraw",
              SameRegion(fullRedraw,longMoved,wxRect(0,190,890,40)));
        model.moveElement("U1",{-120,240}); canvas->OnDocumentReplaced();
        const auto edgeLabel=Snapshot("label-with-body-outside-viewport");
        Check("viewport culling retains visible text when its body is outside",
              !SameRegion(labelBaseline,edgeLabel,wxRect(20,200,60,23)));
        model.loadFrom(editor::Schematic{});
        const auto outsideSwitch=model.addElement("SWITCH",{-100,300});
        const auto outsideLed=model.addElement("LED",{1100,300});
        model.addWire({outsideSwitch,0},{outsideLed,0}); canvas->OnDocumentReplaced();
        wxClientDC wirePixels(canvas); wxColour wireColour;
        Check("viewport culling retains wires crossing from outside endpoints",
              wirePixels.GetPixel(400,300,&wireColour) && wireColour.Red()==0 && wireColour.Green()==95 && wireColour.Blue()==0);

        Reset(); Place("SWITCH", 100, 160); Toggle(100, 160);
        Check("switch starts High before undo audit", sim.query("SW1", 0) == editor::SignalLevel::High);
        Place("AND", 300, 240); canvas->Update(); Snapshot("switch-before-undo");
        canvas->Undo(); Snapshot("switch-after-undo");
        std::cout << "SWITCH_AFTER_UNDO=" << static_cast<int>(sim.query("SW1", 0)) << std::endl;
        Preserved("B-BUG-001 undo of unrelated placement should preserve switch High",
                  sim.query("SW1", 0) == editor::SignalLevel::High);
        canvas->Redo();
        Preserved("B-BUG-001 redo of unrelated placement should preserve switch High",
                  sim.query("SW1", 0) == editor::SignalLevel::High);
        canvas->Undo(); Toggle(100,160); canvas->Redo();
        Check("redo respects a live switch change made after undo",sim.query("SW1",0)==editor::SignalLevel::Low);
        Toggle(100,160); Click(100,160); canvas->DeleteSelection(); canvas->Undo();
        Check("undo deletion restores the removed switch High",sim.query("SW1",0)==editor::SignalLevel::High);
        canvas->Redo(); canvas->Undo();
        Check("repeated delete redo and undo keeps restored switch state",sim.query("SW1",0)==editor::SignalLevel::High);
        model.loadFrom(model.data()); canvas->OnDocumentReplaced(); canvas->ClearHistory();
        Check("document replacement resets switches separately",sim.query("SW1",0)==editor::SignalLevel::Low && !canvas->CanUndo());

        auto* main = new UiMainFrame(); main->Show(); wxYield();
        auto* actualCanvas = Child<CanvasPanel>(main);
        auto* tree = Child<wxTreeCtrl>(main);
        wxTreeItemIdValue cookie;
        tree->SelectItem(tree->GetFirstChild(tree->GetRootItem(), cookie));
        wxYield();
        Check("B-HIS-005 tree selection enables placement", actualCanvas->PlacementType() == "AND");
        actualCanvas->OnDocumentReplaced();
        Check("B-HIS-005 document replacement clears actual tree highlight",
              actualCanvas->PlacementType().empty() && !tree->GetSelection().IsOk());
        FileDialogProbe probe; probe.Register();
        wxCommandEvent open(wxEVT_MENU, wxID_OPEN); main->ProcessWindowEvent(open);
        Check("B-HIS-006 actual Open menu constructs all-files filter first and cancels",
              probe.calls == 1 && probe.wildcard.StartsWith(wxString::FromUTF8("所有文件")));
        probe.Unregister(); main->Destroy();

        std::cout << "SUMMARY passed=" << passed << " unexpected_failures=" << failures
                  << " known_bug_observations=" << knownBugs << std::endl;
        frame->Destroy(); ExitMainLoop();
    }
};
wxIMPLEMENT_APP(BugAudit);
