#include "ui/canvas_panel.h"

#include <wx/wx.h>
#include <wx/dcbuffer.h>

namespace {

editor::PinDescriptor MkPin(const char* name, editor::PinDirection dir, int x, int y) {
    editor::PinDescriptor p;
    p.name = name;
    p.direction = dir;
    p.relPos = editor::Point{x, y};
    return p;
}

editor::Component MkComponent(const char* id, const char* type, int x, int y,
                              std::vector<editor::PinDescriptor> pins) {
    editor::Component c;
    c.id = id;
    c.type = type;
    c.name = id;
    c.pos = editor::Point{x, y};
    c.pins = std::move(pins);
    return c;
}

editor::Wire MkWire(const char* id, const char* fromId, int fromPin,
                    const char* toId, int toPin) {
    editor::Wire w;
    w.id = id;
    w.from = editor::PinRef{fromId, fromPin};
    w.to = editor::PinRef{toId, toPin};
    return w;
}

} // namespace

CanvasPanel::CanvasPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
              wxFULL_REPAINT_ON_RESIZE) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);   // 配合 wxAutoBufferedPaintDC,避免闪烁
    SetBackgroundColour(*wxWHITE);
    SetMinSize(FromDIP(wxSize(400, 300)));

    m_demo = BuildDemoSchematic();          // 阶段四:改成读 SchematicModel::data()

    Bind(wxEVT_PAINT, &CanvasPanel::OnPaint, this);
    Bind(wxEVT_SIZE, &CanvasPanel::OnSize, this);
}

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------

void CanvasPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    DrawGrid(dc);                                        // ① 网格在最底层
    for (const auto& w : m_demo.wires) {                 // ② 导线
        DrawWire(dc, w);
    }
    for (const auto& c : m_demo.components) {            // ③ 元件压在导线之上
        DrawComponent(dc, c);
    }
}

void CanvasPanel::OnSize(wxSizeEvent& evt) {
    Refresh();
    evt.Skip();
}

// ---------------------------------------------------------------------------
// 坐标换算
// ---------------------------------------------------------------------------

wxPoint CanvasPanel::ToScreen(const editor::Point& p) const {
    return wxPoint(m_origin.x + static_cast<int>(p.x * m_scale),
                   m_origin.y + static_cast<int>(p.y * m_scale));
}

editor::Point CanvasPanel::ToLogical(const wxPoint& p) const {
    return editor::Point{static_cast<int>((p.x - m_origin.x) / m_scale),
                         static_cast<int>((p.y - m_origin.y) / m_scale)};
}

// ---------------------------------------------------------------------------
// 绘制
// ---------------------------------------------------------------------------

void CanvasPanel::DrawGrid(wxDC& dc) const {
    const wxSize sz = GetClientSize();

    dc.SetPen(wxPen(wxColour(235, 235, 235), 1));        // 细线
    for (int x = 0; x <= sz.x; x += kGridStep) {
        dc.DrawLine(x, 0, x, sz.y);
    }
    for (int y = 0; y <= sz.y; y += kGridStep) {
        dc.DrawLine(0, y, sz.x, y);
    }

    dc.SetPen(wxPen(wxColour(215, 215, 215), 1));        // 粗线
    for (int x = 0; x <= sz.x; x += kGridStep * kGridMajor) {
        dc.DrawLine(x, 0, x, sz.y);
    }
    for (int y = 0; y <= sz.y; y += kGridStep * kGridMajor) {
        dc.DrawLine(0, y, sz.x, y);
    }
}

void CanvasPanel::DrawComponent(wxDC& dc, const editor::Component& c) const {
    const wxPoint o = ToScreen(c.pos);
    const int hw = kSymW / 2;
    const int hh = kSymH / 2;

    // 元件外形:先用统一矩形占位,阶段四再按类型画真符号
    dc.SetPen(wxPen(wxColour(40, 40, 40), 2));
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.DrawRectangle(o.x - hw, o.y - hh, kSymW, kSymH);

    dc.SetFont(wxFontInfo(8));
    for (const auto& pin : c.pins) {
        const editor::Point abs{c.pos.x + pin.relPos.x, c.pos.y + pin.relPos.y};
        const wxPoint pe = ToScreen(abs);

        // 引脚短线:从矩形边框连到引脚端点
        wxPoint edge = o;
        if (pe.x < o.x - hw) {
            edge = wxPoint(o.x - hw, pe.y);
        } else if (pe.x > o.x + hw) {
            edge = wxPoint(o.x + hw, pe.y);
        } else if (pe.y < o.y - hh) {
            edge = wxPoint(pe.x, o.y - hh);
        } else {
            edge = wxPoint(pe.x, o.y + hh);
        }

        dc.SetPen(wxPen(wxColour(40, 40, 40), 2));
        dc.DrawLine(edge, pe);

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(*wxBLACK_BRUSH);
        dc.DrawRectangle(pe.x - 2, pe.y - 2, 4, 4);      // 引脚端点(阶段三命中锚点)

        dc.SetTextForeground(wxColour(120, 120, 120));
        dc.DrawText(wxString::FromUTF8(pin.name.c_str()), pe.x + 5, pe.y - 16);
    }

    const std::string& label = c.name.empty() ? c.id : c.name;
    dc.SetTextForeground(wxColour(30, 30, 30));
    dc.DrawText(wxString::FromUTF8(label.c_str()), o.x - hw, o.y - hh - 18);
}

void CanvasPanel::DrawWire(wxDC& dc, const editor::Wire& w) const {
    const editor::Component* a = FindComponent(m_demo, w.from.componentId);
    const editor::Component* b = FindComponent(m_demo, w.to.componentId);
    if (a == nullptr || b == nullptr) {
        return;
    }
    if (w.from.pinIndex < 0 || w.from.pinIndex >= static_cast<int>(a->pins.size()) ||
        w.to.pinIndex < 0 || w.to.pinIndex >= static_cast<int>(b->pins.size())) {
        return;
    }

    const editor::PinDescriptor& pa = a->pins[static_cast<size_t>(w.from.pinIndex)];
    const editor::PinDescriptor& pb = b->pins[static_cast<size_t>(w.to.pinIndex)];

    const editor::Point ap{a->pos.x + pa.relPos.x, a->pos.y + pa.relPos.y};
    const editor::Point bp{b->pos.x + pb.relPos.x, b->pos.y + pb.relPos.y};

    dc.SetPen(wxPen(wxColour(0, 120, 0), 2));
    dc.DrawLine(ToScreen(ap), ToScreen(bp));
}

// ---------------------------------------------------------------------------
// 阶段二假数据(阶段四删除)
// ---------------------------------------------------------------------------

const editor::Component* CanvasPanel::FindComponent(const editor::Schematic& s,
                                                    const std::string& id) {
    for (const auto& c : s.components) {
        if (c.id == id) {
            return &c;
        }
    }
    return nullptr;
}

editor::Schematic CanvasPanel::BuildDemoSchematic() {
    constexpr int kOut = kSymW / 2 + kPinLen;    // +50
    constexpr int kIn = -(kSymW / 2 + kPinLen);  // -50

    editor::Schematic s;

    // 两个开关 -> 与门 -> LED(与 docs/requirement.md 的 MVP 演示链路一致)
    s.components.push_back(MkComponent(
        "SW1", "SWITCH", 100, 160,
        {MkPin("Y", editor::PinDirection::Output, kOut, 0)}));
    s.components.push_back(MkComponent(
        "SW2", "SWITCH", 100, 300,
        {MkPin("Y", editor::PinDirection::Output, kOut, 0)}));
    s.components.push_back(MkComponent(
        "U1", "AND", 280, 230,
        {MkPin("A", editor::PinDirection::Input, kIn, -10),
         MkPin("B", editor::PinDirection::Input, kIn, 10),
         MkPin("Y", editor::PinDirection::Output, kOut, 0)}));
    s.components.push_back(MkComponent(
        "LED1", "LED", 460, 230,
        {MkPin("A", editor::PinDirection::Input, kIn, 0)}));

    s.wires.push_back(MkWire("w1", "SW1", 0, "U1", 0));
    s.wires.push_back(MkWire("w2", "SW2", 0, "U1", 1));
    s.wires.push_back(MkWire("w3", "U1", 2, "LED1", 0));

    return s;
}
