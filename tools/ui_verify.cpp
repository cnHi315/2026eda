// GUI 端到端验证台(开发工具,不进构建;不影响 CircuitEditor 的编译)。
//
// 为什么需要它:本机是 Wayland 会话,XTest 注入鼠标无效(指针 warp 不生效、
// 点击投递不到 Xwayland 客户端),所以改成**在进程内直接向 wx 事件系统注入
// wxMouseEvent / wxKeyEvent** —— 窗口与渲染都是真的,只是绕过 X 输入层,
// 外部再用 XGetImage 抓窗口像素做断言。
//
// 编译(不需要改 CMakeLists):
//   g++ -std=c++17 -I src $(wx-config --cxxflags) tools/ui_verify.cpp \
//       src/ui/*.cpp src/model/*.cpp src/components/*.cpp src/io/*.cpp \
//       src/simulation/*.cpp $(wx-config --libs core,base,propgrid) -o /tmp/ui_verify
//
// 命令(每行一条,执行完回 "OK <原命令>"):
//   place <TYPE> <x> <y>    等价"点元件树选中类型 → 点画布放置"
//   down / move / up <x> <y>
//   dclick <x> <y>          双击(拨开关)
//   mdown / mup <x> <y>     中键(平移视图;中间的移动用 move 命令)
//   wheel <x> <y> <rot> [h] 滚轮(rot=±120 为一格;h=1 表示横向轴)
//   perf <n> <motions>      放 n 个元件后拖一个元件跑 motions 次移动,打印耗时
//   esc | del
//   info                    打印标题 / 状态栏两格 / 画布屏幕位置与尺寸
//   quit

#include <wx/wx.h>
#include <wx/stopwatch.h>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

#include "ui/canvas_panel.h"
#include "ui/main_frame.h"

namespace {

CanvasPanel* FindCanvas(wxWindow* w) {
    if (auto* c = dynamic_cast<CanvasPanel*>(w)) {
        return c;
    }
    for (wxWindow* child : w->GetChildren()) {
        if (CanvasPanel* c = FindCanvas(child)) {
            return c;
        }
    }
    return nullptr;
}

} // namespace

class VerifyApp : public wxApp {
public:
    bool OnInit() override {
        frame = new UiMainFrame();
        frame->Show();
        canvas = FindCanvas(frame);
        if (canvas == nullptr) {
            std::printf("FATAL: 找不到 CanvasPanel\n");
            return false;
        }
        timer.Bind(wxEVT_TIMER, &VerifyApp::OnTick, this);
        timer.Start(120);
        return true;
    }

private:
    void SendMouse(wxEventType type, int x, int y) {
        wxMouseEvent evt(type);
        evt.SetEventObject(canvas);
        evt.SetPosition(wxPoint(x, y));
        if (type == wxEVT_LEFT_DOWN || type == wxEVT_LEFT_DCLICK) {
            evt.SetLeftDown(true);
        }
        canvas->ProcessWindowEvent(evt);
    }

    void OnTick(wxTimerEvent&) {
        std::string line;
        if (!std::getline(std::cin, line)) {
            frame->Close(true);
            return;
        }
        std::istringstream is(line);
        std::string cmd;
        is >> cmd;

        if (cmd == "quit") {
            std::printf("OK %s\n", line.c_str());
            std::fflush(stdout);
            frame->Close(true);
            return;
        }
        if (cmd == "place") {
            std::string type;
            int x = 0;
            int y = 0;
            is >> type >> x >> y;
            canvas->SetPlacementType(type);
            SendMouse(wxEVT_LEFT_DOWN, x, y);
            SendMouse(wxEVT_LEFT_UP, x, y);
        } else if (cmd == "down" || cmd == "move" || cmd == "up") {
            int x = 0;
            int y = 0;
            is >> x >> y;
            SendMouse((cmd == "down") ? wxEVT_LEFT_DOWN
                                      : (cmd == "up") ? wxEVT_LEFT_UP : wxEVT_MOTION,
                      x, y);
        } else if (cmd == "dclick") {
            int x = 0;
            int y = 0;
            is >> x >> y;
            SendMouse(wxEVT_LEFT_DOWN, x, y);
            SendMouse(wxEVT_LEFT_UP, x, y);
            SendMouse(wxEVT_LEFT_DCLICK, x, y);
        } else if (cmd == "mdown" || cmd == "mup") {
            int x = 0;
            int y = 0;
            is >> x >> y;
            wxMouseEvent evt((cmd == "mdown") ? wxEVT_MIDDLE_DOWN : wxEVT_MIDDLE_UP);
            evt.SetEventObject(canvas);
            evt.SetPosition(wxPoint(x, y));
            if (cmd == "mdown") {
                evt.SetMiddleDown(true);
            }
            canvas->ProcessWindowEvent(evt);
        } else if (cmd == "wheel") {
            int x = 0;
            int y = 0;
            int rot = 0;
            int horizontal = 0;
            is >> x >> y >> rot >> horizontal;
            wxMouseEvent evt(wxEVT_MOUSEWHEEL);
            evt.SetEventObject(canvas);
            evt.SetPosition(wxPoint(x, y));
            evt.m_wheelRotation = rot;
            evt.m_wheelDelta = 120;
            evt.m_linesPerAction = 1;
            evt.m_wheelAxis = horizontal ? wxMOUSE_WHEEL_HORIZONTAL : wxMOUSE_WHEEL_VERTICAL;
            canvas->ProcessWindowEvent(evt);
        } else if (cmd == "perf") {
            int n = 0;
            int motions = 0;
            is >> n >> motions;
            canvas->SetPlacementType("AND");
            for (int i = 0; i < n; ++i) {
                const int gx = 60 + (i % 12) * 90;
                const int gy = 60 + (i / 12) * 70;
                SendMouse(wxEVT_LEFT_DOWN, gx, gy);
                SendMouse(wxEVT_LEFT_UP, gx, gy);
            }
            canvas->SetPlacementType("");
            wxStopWatch watch;
            SendMouse(wxEVT_LEFT_DOWN, 60, 60);
            for (int i = 0; i < motions; ++i) {
                SendMouse(wxEVT_MOTION, 60 + i, 60 + (i % 20));
                canvas->Update();                        // 把绘制时间也算进去
            }
            SendMouse(wxEVT_LEFT_UP, 60 + motions, 60);
            const long ms = watch.Time();
            std::printf("PERF: n=%d motions=%d total=%ldms avg=%.2fms\n",
                        n, motions, ms, motions > 0 ? double(ms) / motions : 0.0);
        } else if (cmd == "esc" || cmd == "del") {
            wxKeyEvent evt(wxEVT_KEY_DOWN);
            evt.SetEventObject(canvas);
            evt.m_keyCode = (cmd == "esc") ? WXK_ESCAPE : WXK_DELETE;
            canvas->ProcessWindowEvent(evt);
        }

        canvas->Refresh();
        canvas->Update();
        wxTheApp->ProcessPendingEvents();

        if (cmd == "info") {
            wxFrame* top = static_cast<wxFrame*>(frame);
            std::printf("TITLE: %s\n", static_cast<const char*>(top->GetTitle().utf8_str()));
            std::printf("STATUS0: %s\n",
                        static_cast<const char*>(top->GetStatusBar()->GetStatusText(0).utf8_str()));
            std::printf("STATUS1: %s\n",
                        static_cast<const char*>(top->GetStatusBar()->GetStatusText(1).utf8_str()));
            const wxPoint origin = canvas->ClientToScreen(wxPoint(0, 0));
            const wxSize size = canvas->GetClientSize();
            const wxPoint frameOrigin = top->GetScreenPosition();
            std::printf("CANVAS: %d %d %d %d (frame origin %d %d)\n",
                        origin.x, origin.y, size.x, size.y, frameOrigin.x, frameOrigin.y);
        }

        std::printf("OK %s\n", line.c_str());
        std::fflush(stdout);
    }

    UiMainFrame* frame = nullptr;
    CanvasPanel* canvas = nullptr;
    wxTimer timer;
};

wxIMPLEMENT_APP(VerifyApp);
