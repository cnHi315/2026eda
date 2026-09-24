#include "ui/canvas_panel.h"

#include <wx/wx.h>
#include <wx/dcbuffer.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace {

wxString U8(const char* s) {
    return wxString::FromUTF8(s);
}

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

/// 点到线段的距离(逻辑坐标);用于导线命中检测。
double DistancePointToSegment(const editor::Point& p, const editor::Point& a,
                              const editor::Point& b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 <= 0.0) {
        return std::hypot(p.x - a.x, p.y - a.y);
    }
    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
    t = std::max(0.0, std::min(1.0, t));
    return std::hypot(p.x - (a.x + t * dx), p.y - (a.y + t * dy));
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
    Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnLeftDown, this);
    Bind(wxEVT_MOTION, &CanvasPanel::OnMotion, this);
    Bind(wxEVT_LEFT_UP, &CanvasPanel::OnLeftUp, this);
    Bind(wxEVT_LEAVE_WINDOW, &CanvasPanel::OnLeaveWindow, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &CanvasPanel::OnCaptureLost, this);
    Bind(wxEVT_KEY_DOWN, &CanvasPanel::OnKeyDown, this);
}

void CanvasPanel::SetStatusCallback(StatusCallback cb) {
    m_status = std::move(cb);
}

void CanvasPanel::SetStatus(const wxString& text) {
    if (m_status) {
        m_status(text);
    }
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
    DrawRubberBand(dc);                                  // ④ 连线预览在最上层
}

void CanvasPanel::OnSize(wxSizeEvent& evt) {
    Refresh();
    evt.Skip();
}

void CanvasPanel::OnLeftDown(wxMouseEvent& evt) {
    SetFocus();   // 让 Esc 这类按键事件能送到本面板

    const editor::Point p = ToLogical(evt.GetPosition());
    m_mouseLogical = p;
    const HitResult hit = HitTest(p);

    if (hit.IsPin()) {                                   // —— 从引脚起一条线 ——
        m_mode = Mode::DrawingWire;
        m_wireFrom = editor::PinRef{hit.id, hit.pinIndex};
        m_wireSnap = HitResult{};
        m_sel = hit;
        if (!HasCapture()) {
            CaptureMouse();
        }
        SetStatus(U8("连线中 —— 拖到目标引脚,再松开;Esc 取消"));
        Refresh();
        return;
    }

    if (hit.IsComponent()) {                             // —— 拖元件 ——
        const editor::Component* c = FindComponent(m_demo, hit.id);
        if (c == nullptr) {
            return;
        }
        m_mode = Mode::DraggingComponent;
        m_dragId = hit.id;
        m_dragOrig = c->pos;
        m_dragGrab = editor::Point{p.x - c->pos.x, p.y - c->pos.y};
        m_dragMoved = false;
        m_sel = hit;
        if (!HasCapture()) {
            CaptureMouse();
        }
        SetStatus(U8("拖动 ") + wxString::FromUTF8(hit.id.c_str()));
        Refresh();
        return;
    }

    m_sel = hit;                                         // —— 选中导线 / 点空白清空 ——
    if (hit.IsWire()) {
        SetStatus(U8("选中导线 ") + wxString::FromUTF8(hit.id.c_str()));
    } else {
        SetStatus(U8("就绪 —— 拖动元件,或从引脚拖出连线"));
    }
    Refresh();
}

void CanvasPanel::OnMotion(wxMouseEvent& evt) {
    const editor::Point p = ToLogical(evt.GetPosition());
    m_mouseLogical = p;

    if (m_mode == Mode::DraggingComponent) {             // 实时跟随(吸附到栅格)
        const editor::Point raw{p.x - m_dragGrab.x, p.y - m_dragGrab.y};
        if (!m_dragMoved &&
            std::abs(raw.x - m_dragOrig.x) <= 3 && std::abs(raw.y - m_dragOrig.y) <= 3) {
            return;                                      // 仍在"单击抖动"范围内:先不动
        }
        m_dragMoved = true;
        MoveComponentTo(m_dragId, SnapToGrid(raw));
        Refresh();
        return;
    }

    if (m_mode == Mode::DrawingWire) {                   // 橡皮筋 + 目标引脚吸附
        const HitResult hit = HitTestPin(p);
        if (!hit.SameAs(m_wireSnap)) {
            m_wireSnap = hit;
            if (hit.IsPin()) {
                wxString reason;
                const editor::PinRef to{hit.id, hit.pinIndex};
                SetStatus(CanConnect(m_wireFrom, to, &reason)
                              ? U8("可连接 —— 松开完成")
                              : U8("不可连接:") + reason);
            } else {
                SetStatus(U8("连线中 —— 拖到目标引脚,再松开;Esc 取消"));
            }
        }
        Refresh();
        return;
    }

    const HitResult hit = HitTest(p);                    // 空闲态:更新悬停高亮
    if (!hit.SameAs(m_hover)) {
        m_hover = hit;
        Refresh();
    }
    evt.Skip();
}

void CanvasPanel::OnLeftUp(wxMouseEvent& evt) {
    const editor::Point p = ToLogical(evt.GetPosition());

    if (m_mode == Mode::DraggingComponent) {
        const std::string id = m_dragId;
        if (m_dragMoved) {
            const editor::Point target =
                SnapToGrid(editor::Point{p.x - m_dragGrab.x, p.y - m_dragGrab.y});
            MoveComponentTo(id, target);                 // 抬起提交(阶段四 → model.moveElement)
            SetStatus(U8("放下 ") + wxString::FromUTF8(id.c_str()) + U8(" 位置:(") +
                      wxString::Format("%d,%d", target.x, target.y) + U8(")"));
        } else {
            MoveComponentTo(id, m_dragOrig);             // 单击:不产生位移,只选中
            SetStatus(U8("选中 ") + wxString::FromUTF8(id.c_str()));
        }
        EndInteraction();
        Refresh();
        return;
    }

    if (m_mode == Mode::DrawingWire) {
        const HitResult hit = HitTestPin(p);
        if (hit.IsPin()) {
            const editor::PinRef to{hit.id, hit.pinIndex};
            wxString reason;
            if (CanConnect(m_wireFrom, to, &reason)) {
                AddWire(m_wireFrom, to);                 // 阶段四 → model.addWire
            } else {
                SetStatus(U8("连线未创建:") + reason);
            }
        } else {
            SetStatus(U8("已取消连线"));
        }
        EndInteraction();
        Refresh();
        return;
    }

    evt.Skip();
}

void CanvasPanel::OnLeaveWindow(wxMouseEvent& evt) {
    if (m_mode == Mode::Idle && !m_hover.IsNone()) {
        m_hover = HitResult{};
        Refresh();
    }
    evt.Skip();
}

void CanvasPanel::OnCaptureLost(wxMouseCaptureLostEvent&) {
    // 系统抢走鼠标捕获时必须复位,否则状态机会卡在拖拽/连线态。
    if (m_mode != Mode::Idle) {
        CancelInteraction(U8("操作已取消"));
        Refresh();
    }
}

void CanvasPanel::OnKeyDown(wxKeyEvent& evt) {
    if (evt.GetKeyCode() == WXK_ESCAPE && m_mode != Mode::Idle) {
        if (m_mode == Mode::DraggingComponent) {
            MoveComponentTo(m_dragId, m_dragOrig);       // Esc:把元件放回原位
            CancelInteraction(U8("已取消拖动(位置已还原)"));
        } else {
            CancelInteraction(U8("已取消连线"));
        }
        Refresh();
        return;
    }
    evt.Skip();
}

void CanvasPanel::EndInteraction() {
    if (HasCapture()) {
        ReleaseMouse();
    }
    m_mode = Mode::Idle;
    m_dragId.clear();
    m_wireSnap = HitResult{};
}

void CanvasPanel::CancelInteraction(const wxString& status) {
    EndInteraction();
    SetStatus(status);
}

// ---------------------------------------------------------------------------
// 命中检测(优先级:引脚 → 元件 → 导线)
// ---------------------------------------------------------------------------

CanvasPanel::HitResult CanvasPanel::HitTest(const editor::Point& logical) const {
    HitResult hit = HitTestPin(logical);
    if (!hit.IsNone()) {
        return hit;
    }
    hit = HitTestComponent(logical);
    if (!hit.IsNone()) {
        return hit;
    }
    return HitTestWire(logical);
}

CanvasPanel::HitResult CanvasPanel::HitTestPin(const editor::Point& logical) const {
    HitResult best;
    double bestDist = static_cast<double>(kPinHitR);

    for (const auto& c : m_demo.components) {
        for (size_t i = 0; i < c.pins.size(); ++i) {
            const editor::Point abs{c.pos.x + c.pins[i].relPos.x,
                                    c.pos.y + c.pins[i].relPos.y};
            const double d = std::hypot(logical.x - abs.x, logical.y - abs.y);
            if (d <= bestDist) {
                bestDist = d;
                best.kind = HitKind::Pin;
                best.id = c.id;
                best.pinIndex = static_cast<int>(i);
            }
        }
    }
    return best;
}

CanvasPanel::HitResult CanvasPanel::HitTestComponent(const editor::Point& logical) const {
    for (const auto& c : m_demo.components) {
        const SymbolSize sz = SizeOf(c.type);
        const int hw = sz.w / 2;
        const int hh = sz.h / 2;
        if (logical.x >= c.pos.x - hw && logical.x <= c.pos.x + hw &&
            logical.y >= c.pos.y - hh && logical.y <= c.pos.y + hh) {
            HitResult hit;
            hit.kind = HitKind::Component;
            hit.id = c.id;
            return hit;
        }
    }
    return HitResult{};
}

CanvasPanel::HitResult CanvasPanel::HitTestWire(const editor::Point& logical) const {
    for (const auto& w : m_demo.wires) {
        editor::Point a;
        editor::Point b;
        if (!PinRefLogicalPos(w.from, &a) || !PinRefLogicalPos(w.to, &b)) {
            continue;
        }
        if (DistancePointToSegment(logical, a, b) <= static_cast<double>(kWireHitR)) {
            HitResult hit;
            hit.kind = HitKind::Wire;
            hit.id = w.id;
            return hit;
        }
    }
    return HitResult{};
}

// ---------------------------------------------------------------------------
// 数据写入(阶段四换成 SchematicModel 的调用)
// ---------------------------------------------------------------------------

void CanvasPanel::MoveComponentTo(const std::string& id, const editor::Point& pos) {
    for (auto& c : m_demo.components) {
        if (c.id == id) {
            c.pos = pos;                                 // 阶段四 → model.moveElement(id, pos)
            return;
        }
    }
}

bool CanvasPanel::CanConnect(const editor::PinRef& from, const editor::PinRef& to,
                             wxString* reason) const {
    // 校验规则与 A 的 SchematicModel::addWire 保持一致(见 docs/data-model.md)。
    editor::Point a;
    editor::Point b;
    if (!PinRefLogicalPos(from, &a) || !PinRefLogicalPos(to, &b)) {
        *reason = U8("引脚不存在");
        return false;
    }
    if (from.componentId == to.componentId && from.pinIndex == to.pinIndex) {
        *reason = U8("不能连到同一个引脚(自环)");
        return false;
    }
    const editor::Component* ca = FindComponent(m_demo, from.componentId);
    const editor::Component* cb = FindComponent(m_demo, to.componentId);
    if (ca != nullptr && cb != nullptr &&
        IsOutputPin(*ca, from.pinIndex) && IsOutputPin(*cb, to.pinIndex)) {
        *reason = U8("不允许两个输出引脚直连");
        return false;
    }
    for (const auto& w : m_demo.wires) {
        const bool same = w.from.componentId == from.componentId &&
                          w.from.pinIndex == from.pinIndex &&
                          w.to.componentId == to.componentId &&
                          w.to.pinIndex == to.pinIndex;
        const bool reversed = w.from.componentId == to.componentId &&
                              w.from.pinIndex == to.pinIndex &&
                              w.to.componentId == from.componentId &&
                              w.to.pinIndex == from.pinIndex;
        if (same || reversed) {                          // 重复连线(正反都算)
            *reason = U8("这条连线已存在");
            return false;
        }
    }
    return true;
}

void CanvasPanel::AddWire(const editor::PinRef& from, const editor::PinRef& to) {
    size_t n = m_demo.wires.size() + 1;
    std::string id = "w" + std::to_string(n);
    while (std::any_of(m_demo.wires.begin(), m_demo.wires.end(),
                       [&id](const editor::Wire& w) { return w.id == id; })) {
        id = "w" + std::to_string(++n);
    }

    editor::Wire w;                                      // 阶段四 → model.addWire(from, to)
    w.id = id;
    w.from = from;
    w.to = to;
    m_demo.wires.push_back(w);

    HitResult sel;                                       // 新线高亮一下,给个反馈
    sel.kind = HitKind::Wire;
    sel.id = id;
    m_sel = sel;
    SetStatus(U8("已连线 ") + wxString::FromUTF8(from.componentId.c_str()) +
              wxString::Format("#%d", from.pinIndex) + U8(" → ") +
              wxString::FromUTF8(to.componentId.c_str()) +
              wxString::Format("#%d", to.pinIndex));
}

// ---------------------------------------------------------------------------
// 几何辅助
// ---------------------------------------------------------------------------

CanvasPanel::SymbolSize CanvasPanel::SizeOf(const std::string& type) {
    // 当前五个元件统一 60×40 + 引脚长 20 —— 与 C 的 pinTemplate relPos ±50 自洽
    // (±50 = kSymW/2 + kPinLen)。Issue 3:与 C 定死各类型包围盒后,这里换成逐类型查表。
    (void)type;
    return SymbolSize{kSymW, kSymH, kPinLen};
}

bool CanvasPanel::PinRefLogicalPos(const editor::PinRef& pin, editor::Point* out) const {
    const editor::Component* c = FindComponent(m_demo, pin.componentId);
    if (c == nullptr || pin.pinIndex < 0 ||
        pin.pinIndex >= static_cast<int>(c->pins.size())) {
        return false;
    }
    const editor::PinDescriptor& pd = c->pins[static_cast<size_t>(pin.pinIndex)];
    *out = editor::Point{c->pos.x + pd.relPos.x, c->pos.y + pd.relPos.y};
    return true;
}

editor::Point CanvasPanel::SnapToGrid(const editor::Point& p) {
    const auto snap = [](int v) {
        return static_cast<int>(std::lround(static_cast<double>(v) / kGridStep)) * kGridStep;
    };
    return editor::Point{snap(p.x), snap(p.y)};
}

bool CanvasPanel::IsOutputPin(const editor::Component& c, int pinIndex) {
    if (pinIndex < 0 || pinIndex >= static_cast<int>(c.pins.size())) {
        return false;
    }
    return c.pins[static_cast<size_t>(pinIndex)].direction == editor::PinDirection::Output;
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

void CanvasPanel::DrawPinDot(wxDC& dc, const wxPoint& p, bool highlight) const {
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(highlight ? wxBrush(wxColour(230, 120, 0)) : *wxBLACK_BRUSH);
    dc.DrawCircle(p, highlight ? 5 : 2);
}

void CanvasPanel::DrawComponent(wxDC& dc, const editor::Component& c) const {
    const SymbolSize sz = SizeOf(c.type);
    const wxPoint o = ToScreen(c.pos);
    const int hw = sz.w / 2;
    const int hh = sz.h / 2;
    const bool selected = m_sel.IsComponent() && m_sel.id == c.id;
    const bool hovered = m_hover.IsComponent() && m_hover.id == c.id;

    // 选中/悬停:蓝色虚线外框(先画,免得盖住元件体)
    if (selected || hovered) {
        dc.SetPen(wxPen(wxColour(0, 90, 200), selected ? 2 : 1,
                        selected ? wxPENSTYLE_SOLID : wxPENSTYLE_DOT));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(o.x - hw - 5, o.y - hh - 5, sz.w + 10, sz.h + 10);
    }

    // 元件外形:先用统一矩形占位,阶段四再按类型画真符号
    dc.SetPen(wxPen(wxColour(40, 40, 40), 2));
    dc.SetBrush(*wxWHITE_BRUSH);
    dc.DrawRectangle(o.x - hw, o.y - hh, sz.w, sz.h);

    dc.SetFont(wxFontInfo(8));
    for (size_t i = 0; i < c.pins.size(); ++i) {
        const editor::PinDescriptor& pin = c.pins[i];
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

        const bool pinSelected = m_sel.IsPin() && m_sel.id == c.id &&
                                 m_sel.pinIndex == static_cast<int>(i);
        const bool pinHovered = m_hover.IsPin() && m_hover.id == c.id &&
                                m_hover.pinIndex == static_cast<int>(i);
        const bool pinSnapped = m_mode == Mode::DrawingWire && m_wireSnap.IsPin() &&
                                m_wireSnap.id == c.id &&
                                m_wireSnap.pinIndex == static_cast<int>(i);
        DrawPinDot(dc, pe, pinSelected || pinHovered || pinSnapped);

        dc.SetTextForeground(wxColour(120, 120, 120));
        dc.DrawText(U8(pin.name.c_str()), pe.x + 5, pe.y - 16);
    }

    const std::string& label = c.name.empty() ? c.id : c.name;
    dc.SetTextForeground(wxColour(30, 30, 30));
    dc.DrawText(U8(label.c_str()), o.x - hw, o.y - hh - 18);
}

void CanvasPanel::DrawWire(wxDC& dc, const editor::Wire& w) const {
    editor::Point a;
    editor::Point b;
    if (!PinRefLogicalPos(w.from, &a) || !PinRefLogicalPos(w.to, &b)) {
        return;
    }
    const bool selected = m_sel.IsWire() && m_sel.id == w.id;
    dc.SetPen(selected ? wxPen(wxColour(0, 90, 200), 4) : wxPen(wxColour(0, 120, 0), 2));
    dc.DrawLine(ToScreen(a), ToScreen(b));
}

void CanvasPanel::DrawRubberBand(wxDC& dc) const {
    if (m_mode != Mode::DrawingWire) {
        return;
    }
    editor::Point start;
    if (!PinRefLogicalPos(m_wireFrom, &start)) {
        return;
    }

    editor::Point end = m_mouseLogical;                  // 未吸附时跟随鼠标
    if (m_wireSnap.IsPin()) {
        editor::Point snapped;
        if (PinRefLogicalPos(editor::PinRef{m_wireSnap.id, m_wireSnap.pinIndex}, &snapped)) {
            end = snapped;
        }
    }

    dc.SetPen(wxPen(wxColour(0, 120, 0), 2, wxPENSTYLE_SHORT_DASH));
    dc.DrawLine(ToScreen(start), ToScreen(end));
    dc.SetPen(wxPen(wxColour(0, 120, 0), 2));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawCircle(ToScreen(end), 4);
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
