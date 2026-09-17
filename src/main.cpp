// 程序入口(负责人 B)。窗口与界面组件见 src/ui/。
#include <wx/wx.h>

#include "ui/main_frame.h"

class CircuitApp : public wxApp {
public:
    bool OnInit() override;
};

wxIMPLEMENT_APP(CircuitApp);

bool CircuitApp::OnInit() {
    auto* frame = new UiMainFrame();
    frame->Show(true);
    return true;
}
