#include <wx/wx.h>
#include <wx/artprov.h>

class CircuitApp : public wxApp {
public:
    bool OnInit() override;
};

class MainFrame : public wxFrame {
public:
    MainFrame()
        : wxFrame(nullptr, wxID_ANY, "Circuit Editor",
                  wxDefaultPosition, wxSize(800, 600)) {
        BuildMenuBar();
        BuildToolBar();
        CreateStatusBar();
        SetStatusText("Ready");
    }

private:
    void BuildMenuBar() {
        auto* fileMenu = new wxMenu;
        fileMenu->Append(wxID_EXIT, "E&xit\tCtrl+Q", "Quit the application");

        auto* bar = new wxMenuBar;
        bar->Append(fileMenu, "&File");
        SetMenuBar(bar);

        Bind(wxEVT_MENU, [this](wxCommandEvent&) { Close(true); }, wxID_EXIT);
    }

    void BuildToolBar() {
        auto* tb = CreateToolBar();
        tb->AddTool(wxID_NEW, "New",
                    wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR),
                    "New circuit");
        tb->Realize();
    }
};

wxIMPLEMENT_APP(CircuitApp);

bool CircuitApp::OnInit() {
    auto* frame = new MainFrame();
    frame->Show(true);
    return true;
}