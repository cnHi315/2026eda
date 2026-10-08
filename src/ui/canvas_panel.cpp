#include "ui/canvas_panel.h"

#include <wx/wx.h>
#include <wx/dcbuffer.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "model/schematic_model.h"
#include "simulation/simulator.h"

namespace {

wxString U8(const char* s) {
    return wxString::FromUTF8(s);
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

    Bind(wxEVT_PAINT, &CanvasPanel::OnPaint, this);
    Bind(wxEVT_SIZE, &CanvasPanel::OnSize, this);
    Bind(wxEVT_LEFT_DOWN, &CanvasPanel::OnLeftDown, this);
    Bind(wxEVT_LEFT_DCLICK, &CanvasPanel::OnLeftDClick, this);
    Bind(wxEVT_MOTION, &CanvasPanel::OnMotion, this);
    Bind(wxEVT_LEFT_UP, &CanvasPanel::OnLeftUp, this);
    Bind(wxEVT_MIDDLE_DOWN, &CanvasPanel::OnMiddleDown, this);
    Bind(wxEVT_MIDDLE_UP, &CanvasPanel::OnMiddleUp, this);
    Bind(wxEVT_MOUSEWHEEL, &CanvasPanel::OnMouseWheel, this);
    Bind(wxEVT_LEAVE_WINDOW, &CanvasPanel::OnLeaveWindow, this);
    Bind(wxEVT_MOUSE_CAPTURE_LOST, &CanvasPanel::OnCaptureLost, this);
    Bind(wxEVT_KEY_DOWN, &CanvasPanel::OnKeyDown, this);
}

void CanvasPanel::Attach(editor::SchematicModel* model, editor::Simulator* sim) {
    m_model = model;
    m_sim = sim;
}

void CanvasPanel::SetStatusCallback(StatusCallback cb) { m_status = std::move(cb); }
void CanvasPanel::SetSelectionCallback(SelectionCallback cb) { m_onSelection = std::move(cb); }
void CanvasPanel::SetChangedCallback(ChangedCallback cb) { m_onChanged = std::move(cb); }

void CanvasPanel::SetPlacementType(const std::string& type) {
    m_placeType = type;
    if (!type.empty()) {
        SetStatus(U8("放置模式:") + wxString::FromUTF8(type.c_str()) +
                  U8(" —— 点击画布空白处放置;Esc 退出"));
    }
    Refresh();
}

void CanvasPanel::ClearPlacementType() {
    if (!m_placeType.empty()) {
        m_placeType.clear();
        SetStatus(U8("已退出放置模式"));
        Refresh();
    }
}

void CanvasPanel::OnDocumentReplaced() {
    EndInteraction();
    m_sel = HitResult{};
    m_hover = HitResult{};
    m_switchLevel.clear();          // 开关默认关闭,且 v1 不持久化
    m_placeType.clear();
    ResyncSimulation();
    NotifySelection();
    NotifyChanged();
    Refresh();
}

void CanvasPanel::ResyncSimulation() {
    ApplySwitchInputs();
}

// ---------------------------------------------------------------------------
// 事件
// ---------------------------------------------------------------------------

void CanvasPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    dc.SetBackground(wxBrush(GetBackgroundColour()));
    dc.Clear();

    DrawGrid(dc);                                        // ① 网格在最底层
    for (const auto& w : S().wires) {                    // ② 导线
        DrawWire(dc, w);
    }
    for (const auto& c : S().components) {               // ③ 元件压在导线之上
        DrawComponent(dc, c);
    }
    DrawRubberBand(dc);                                  // ④ 连线预览在最上层

    if (S().components.empty()) {
        DrawEmptyHint(dc);
    }
}

void CanvasPanel::OnSize(wxSizeEvent& evt) {
    Refresh();
    evt.Skip();
}

void CanvasPanel::OnLeftDown(wxMouseEvent& evt) {
    SetFocus();   // 让 Esc / Delete 这类按键事件能送到本面板

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
        NotifySelection();
        Refresh();
        return;
    }

    if (hit.IsComponent()) {                             // —— 拖元件 ——
        const editor::Component* c = Find(hit.id);
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
        NotifySelection();
        Refresh();
        return;
    }

    if (hit.IsWire()) {                                  // —— 选中导线 ——
        m_sel = hit;
        SetStatus(U8("选中导线 ") + wxString::FromUTF8(hit.id.c_str()));
        NotifySelection();
        Refresh();
        return;
    }

    if (!m_placeType.empty()) {                          // —— 放置元件 ——
        PlaceAt(p);
        return;
    }

    m_sel = HitResult{};                                 // —— 点空白:清空选中 ——
    SetStatus(U8("就绪 —— 拖动元件,或从引脚拖出连线"));
    NotifySelection();
    Refresh();
}

void CanvasPanel::OnLeftDClick(wxMouseEvent& evt) {
    if (m_mode != Mode::Idle) {
        evt.Skip();
        return;
    }
    const HitResult hit = HitTest(ToLogical(evt.GetPosition()));
    if (hit.IsComponent() || hit.IsPin()) {
        const editor::Component* c = Find(hit.id);
        if (c != nullptr && IsSwitch(*c)) {
            ToggleSwitch(hit.id);                        // 双击开关 = 拨动
            return;
        }
    }
    evt.Skip();
}

void CanvasPanel::OnMotion(wxMouseEvent& evt) {
    const editor::Point p = ToLogical(evt.GetPosition());
    m_mouseLogical = p;

    if (m_panning) {                                     // 中键拖动 = 平移视图
        m_origin = m_panStartOrigin + (evt.GetPosition() - m_panStartMouse);
        ClampOrigin();
        Refresh();
        return;
    }

    if (m_mode == Mode::DraggingComponent) {             // 实时跟随(吸附到栅格)
        const editor::Point raw{p.x - m_dragGrab.x, p.y - m_dragGrab.y};
        if (!m_dragMoved &&
            std::abs(raw.x - m_dragOrig.x) <= 3 && std::abs(raw.y - m_dragOrig.y) <= 3) {
            return;                                      // 仍在"单击抖动"范围内:先不动
        }
        m_dragMoved = true;

        wxRect oldArea;                                  // 旧位置也要重绘(局部刷新)
        if (const editor::Component* before = Find(m_dragId)) {
            oldArea = ScreenRectOf(*before, kCmpMargin);
            for (const auto& w : S().wires) {
                if (w.from.componentId == m_dragId || w.to.componentId == m_dragId) {
                    oldArea.Union(ScreenRectOfWire(w, 6));
                }
            }
        }
        MoveComponentTo(m_dragId, SnapToGrid(raw));
        if (!oldArea.IsEmpty()) {
            RefreshRect(oldArea);
        }
        RefreshComponentArea(m_dragId);
        return;
    }

    if (m_mode == Mode::DrawingWire) {                   // 橡皮筋 + 目标引脚吸附
        const HitResult hit = HitTestPin(p);
        const wxRect previousBand = m_bandRect;
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
        m_bandRect = RubberBandScreenRect();
        RefreshRubberBandArea(previousBand);
        return;
    }

    const HitResult hit = HitTest(p);                    // 空闲态:更新悬停高亮
    if (!hit.SameAs(m_hover)) {
        const HitResult previous = m_hover;
        m_hover = hit;
        if (!previous.IsNone()) {
            RefreshComponentArea(previous.id);
        }
        if (!hit.IsNone()) {
            RefreshComponentArea(hit.id);
        }
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
            MoveComponentTo(id, target);                 // model.moveElement
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
        const bool samePin = hit.IsPin() && hit.id == m_wireFrom.componentId &&
                             hit.pinIndex == m_wireFrom.pinIndex;
        if (samePin) {
            SetStatus(U8("已取消连线"));                  // 原地按放(含双击误触)不算错误
        } else if (hit.IsPin()) {
            const editor::PinRef to{hit.id, hit.pinIndex};
            wxString reason;
            if (CanConnect(m_wireFrom, to, &reason)) {
                AddWire(m_wireFrom, to);                 // model.addWire
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
    if (m_panning) {
        m_panning = false;
        SetCursor(wxNullCursor);
        return;
    }
    if (m_mode != Mode::Idle) {
        CancelInteraction(U8("操作已取消"));
        Refresh();
    }
}

void CanvasPanel::OnMiddleDown(wxMouseEvent& evt) {
    SetFocus();
    m_panning = true;
    m_panStartMouse = evt.GetPosition();
    m_panStartOrigin = m_origin;
    if (!HasCapture()) {
        CaptureMouse();
    }
    SetCursor(wxCursor(wxCURSOR_SIZING));
    SetStatus(U8("平移视图 —— 拖动中,松开结束(滚轮也可滚动)"));
    evt.Skip();
}

void CanvasPanel::OnMiddleUp(wxMouseEvent& evt) {
    if (m_panning) {
        m_panning = false;
        if (HasCapture()) {
            ReleaseMouse();
        }
        SetCursor(wxNullCursor);
        SetStatus(wxString::Format(U8("视图已平移:原点偏移 (%d, %d)"), m_origin.x, m_origin.y));
        Refresh();
    }
    evt.Skip();
}

void CanvasPanel::OnMouseWheel(wxMouseEvent& evt) {
    // 滚轮 = 纵向滚动,Shift+滚轮(或触摸板横扫)= 横向滚动;步长取一个栅格
    const int rotation = evt.GetWheelRotation();
    if (rotation == 0) {
        evt.Skip();
        return;
    }
    const int lines = evt.GetLinesPerAction() > 0 ? evt.GetLinesPerAction() : 1;
    const int amount = static_cast<int>((rotation / 120.0) * kGridStep * lines * m_scale);
    const bool horizontal = evt.ShiftDown() || evt.GetWheelAxis() == wxMOUSE_WHEEL_HORIZONTAL;
    PanBy(horizontal ? wxPoint(amount, 0) : wxPoint(0, amount));
    evt.Skip();
}

void CanvasPanel::OnKeyDown(wxKeyEvent& evt) {
    const int code = evt.GetKeyCode();

    if (code == WXK_ESCAPE) {
        if (m_mode == Mode::DraggingComponent) {
            MoveComponentTo(m_dragId, m_dragOrig);       // Esc:把元件放回原位
            CancelInteraction(U8("已取消拖动(位置已还原)"));
            Refresh();
            return;
        }
        if (m_mode == Mode::DrawingWire) {
            CancelInteraction(U8("已取消连线"));
            Refresh();
            return;
        }
        if (!m_placeType.empty()) {
            ClearPlacementType();
            return;
        }
    }

    if (code == WXK_DELETE || code == WXK_NUMPAD_DELETE) {
        DeleteSelection();
        return;
    }

    evt.Skip();
}

// ---------------------------------------------------------------------------
// 写操作(全部走 SchematicModel)
// ---------------------------------------------------------------------------

bool CanvasPanel::PlaceAt(const editor::Point& logical) {
    if (m_model == nullptr || m_placeType.empty()) {
        return false;
    }
    const editor::Point pos = SnapToGrid(logical);
    const std::string id = m_model->addElement(m_placeType, pos);
    if (id.empty()) {
        SetStatus(U8("放置失败:未知元件类型 ") + wxString::FromUTF8(m_placeType.c_str()));
        return false;
    }

    ResyncSimulation();
    HitResult sel;
    sel.kind = HitKind::Component;
    sel.id = id;
    m_sel = sel;
    SetStatus(U8("已放置 ") + wxString::FromUTF8(id.c_str()) + U8("(") +
              wxString::FromUTF8(m_placeType.c_str()) + U8(") 位置:(") +
              wxString::Format("%d,%d", pos.x, pos.y) + U8(")"));
    NotifySelection();
    NotifyChanged();
    Refresh();
    return true;
}

void CanvasPanel::MoveComponentTo(const std::string& id, const editor::Point& pos) {
    if (m_model == nullptr) {
        return;
    }
    const editor::Component* before = Find(id);
    if (before != nullptr && before->pos.x == pos.x && before->pos.y == pos.y) {
        return;                                          // 没变就不动,省一次重绘
    }
    if (m_model->moveElement(id, pos)) {
        NotifyChanged();
    }
}

bool CanvasPanel::CanConnect(const editor::PinRef& from, const editor::PinRef& to,
                             wxString* reason) const {
    // 与 A 的 SchematicModel::addWire 同一套规则(见 docs/data-model.md),
    // 这里只负责给出"为什么不行"的文案;真正的写入仍然交给 model。
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
    const editor::Component* ca = Find(from.componentId);
    const editor::Component* cb = Find(to.componentId);
    if (ca != nullptr && cb != nullptr &&
        IsOutputPin(*ca, from.pinIndex) && IsOutputPin(*cb, to.pinIndex)) {
        *reason = U8("不允许两个输出引脚直连");
        return false;
    }
    for (const auto& w : S().wires) {
        const bool same = w.from.componentId == from.componentId &&
                          w.from.pinIndex == from.pinIndex &&
                          w.to.componentId == to.componentId &&
                          w.to.pinIndex == to.pinIndex;
        const bool reversed = w.from.componentId == to.componentId &&
                              w.from.pinIndex == to.pinIndex &&
                              w.to.componentId == from.componentId &&
                              w.to.pinIndex == from.pinIndex;
        if (same || reversed) {
            *reason = U8("这条连线已存在");
            return false;
        }
    }
    return true;
}

void CanvasPanel::AddWire(const editor::PinRef& from, const editor::PinRef& to) {
    if (m_model == nullptr) {
        return;
    }
    const std::string netId = m_model->addWire(from, to);
    if (netId.empty()) {
        SetStatus(U8("连线未创建:模型拒绝了这条连线"));
        return;
    }

    ResyncSimulation();
    HitResult sel;                                       // 选中刚加的线,给个反馈
    sel.kind = HitKind::Wire;
    for (const auto& w : S().wires) {
        if ((w.from.componentId == from.componentId && w.from.pinIndex == from.pinIndex &&
             w.to.componentId == to.componentId && w.to.pinIndex == to.pinIndex) ||
            (w.from.componentId == to.componentId && w.from.pinIndex == to.pinIndex &&
             w.to.componentId == from.componentId && w.to.pinIndex == from.pinIndex)) {
            sel.id = w.id;
            break;
        }
    }
    m_sel = sel.id.empty() ? HitResult{} : sel;
    SetStatus(U8("已连线 ") + wxString::FromUTF8(from.componentId.c_str()) +
              wxString::Format("#%d", from.pinIndex) + U8(" → ") +
              wxString::FromUTF8(to.componentId.c_str()) +
              wxString::Format("#%d", to.pinIndex) + U8("  网络:") +
              wxString::FromUTF8(netId.c_str()));
    NotifySelection();
    NotifyChanged();
}

void CanvasPanel::ToggleSwitch(const std::string& id) {
    const editor::SignalLevel next =
        (SwitchLevel(id) == editor::SignalLevel::High) ? editor::SignalLevel::Low
                                                       : editor::SignalLevel::High;
    m_switchLevel[id] = next;
    ApplySwitchInputs();
    SetStatus(U8("开关 ") + wxString::FromUTF8(id.c_str()) +
              (next == editor::SignalLevel::High ? U8(" 打开(1)") : U8(" 关闭(0)")));
    NotifyChanged();
    Refresh();
}

bool CanvasPanel::DeleteSelection() {
    if (m_model == nullptr) {
        return false;
    }
    if (m_sel.IsComponent() || m_sel.IsPin()) {
        const std::string id = m_sel.id;
        if (m_model->removeElement(id)) {
            m_switchLevel.erase(id);
            m_sel = HitResult{};
            ResyncSimulation();
            SetStatus(U8("已删除元件 ") + wxString::FromUTF8(id.c_str()));
            NotifySelection();
            NotifyChanged();
            Refresh();
            return true;
        }
        SetStatus(U8("删除失败:") + wxString::FromUTF8(id.c_str()));
        return false;
    }
    if (m_sel.IsWire()) {
        const std::string id = m_sel.id;
        if (m_model->removeWire(id)) {
            m_sel = HitResult{};
            ResyncSimulation();
            SetStatus(U8("已删除导线 ") + wxString::FromUTF8(id.c_str()));
            NotifySelection();
            NotifyChanged();
            Refresh();
            return true;
        }
        SetStatus(U8("删除失败:") + wxString::FromUTF8(id.c_str()));
        return false;
    }
    SetStatus(U8("没有选中任何对象"));
    return false;
}

// ---------------------------------------------------------------------------
// 仿真辅助
// ---------------------------------------------------------------------------

editor::SignalLevel CanvasPanel::SwitchLevel(const std::string& id) const {
    const auto it = m_switchLevel.find(id);
    return it == m_switchLevel.end() ? editor::SignalLevel::Low : it->second;
}

void CanvasPanel::ApplySwitchInputs() {
    if (m_model == nullptr || m_sim == nullptr) {
        return;
    }
    // model data → simulator;再把每个开关的当前挡位重放一遍(默认关闭)。
    m_sim->load(m_model->data());
    for (const auto& c : m_model->data().components) {
        if (!IsSwitch(c)) {
            continue;
        }
        const editor::SignalLevel lv = SwitchLevel(c.id);
        for (size_t i = 0; i < c.pins.size(); ++i) {     // 单脚/两脚模型都兼容
            m_sim->setInput(c.id, static_cast<int>(i), lv);
        }
    }
    m_sim->step();
}

editor::SignalLevel CanvasPanel::PinLevel(const editor::PinRef& pin) const {
    if (m_sim == nullptr) {
        return editor::SignalLevel::Undefined;
    }
    return m_sim->query(pin.componentId, pin.pinIndex);
}

editor::SignalLevel CanvasPanel::WireLevel(const editor::Wire& w) const {
    const editor::SignalLevel from = PinLevel(w.from);
    return from != editor::SignalLevel::Undefined ? from : PinLevel(w.to);
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

    for (const auto& c : S().components) {
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
    for (const auto& c : S().components) {
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
    for (const auto& w : S().wires) {
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
// 几何辅助
// ---------------------------------------------------------------------------

CanvasPanel::SymbolSize CanvasPanel::SizeOf(const std::string& type) {
    // 第 4 周定稿(见 docs/interfaces.md 几何参数表):五种类型统一 60×40 + 引脚长 20,
    // 与 C 的 pinTemplate relPos ±50 自洽(±50 = kSymW/2 + kPinLen)。
    (void)type;
    return SymbolSize{kSymW, kSymH, kPinLen};
}

const editor::Component* CanvasPanel::Find(const std::string& id) const {
    if (m_model == nullptr) {
        return nullptr;
    }
    RebuildIndexIfNeeded();
    const auto it = m_index.find(id);
    return it == m_index.end() ? nullptr : it->second;
}

void CanvasPanel::RebuildIndexIfNeeded() const {
    if (!m_indexDirty) {
        return;
    }
    m_index.clear();
    m_index.reserve(S().components.size());
    for (const auto& c : S().components) {
        m_index.emplace(c.id, &c);
    }
    m_indexDirty = false;
}

const editor::Schematic& CanvasPanel::S() const {
    static const editor::Schematic kEmpty;
    return m_model == nullptr ? kEmpty : m_model->data();
}

bool CanvasPanel::PinRefLogicalPos(const editor::PinRef& pin, editor::Point* out) const {
    const editor::Component* c = Find(pin.componentId);
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

bool CanvasPanel::IsSwitch(const editor::Component& c) { return c.type == "SWITCH"; }
bool CanvasPanel::IsLed(const editor::Component& c) { return c.type == "LED"; }

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
    wxRect clip;
    if (!dc.GetClippingBox(clip) || clip.IsEmpty()) {
        clip = wxRect(0, 0, sz.x, sz.y);
    }

    const int step = static_cast<int>(kGridStep * m_scale);
    const int major = step * kGridMajor;
    const int xEnd = clip.x + clip.width;
    const int yEnd = clip.y + clip.height;

    // 网格线锚在逻辑坐标原点(平移后仍与 20 栅格对齐),并且只在需要重绘的区域内画
    const auto firstLine = [](int from, int origin, int stepPx) {
        const int diff = from - origin;
        int k = diff / stepPx;
        if (diff < 0 && diff % stepPx != 0) {
            --k;
        }
        return origin + k * stepPx;
    };

    dc.SetPen(wxPen(wxColour(235, 235, 235), 1));        // 细线
    for (int x = firstLine(clip.x, m_origin.x, step); x <= xEnd; x += step) {
        dc.DrawLine(x, clip.y, x, yEnd);
    }
    for (int y = firstLine(clip.y, m_origin.y, step); y <= yEnd; y += step) {
        dc.DrawLine(clip.x, y, xEnd, y);
    }

    dc.SetPen(wxPen(wxColour(215, 215, 215), 1));        // 粗线
    for (int x = firstLine(clip.x, m_origin.x, major); x <= xEnd; x += major) {
        dc.DrawLine(x, clip.y, x, yEnd);
    }
    for (int y = firstLine(clip.y, m_origin.y, major); y <= yEnd; y += major) {
        dc.DrawLine(clip.x, y, xEnd, y);
    }
}

void CanvasPanel::DrawEmptyHint(wxDC& dc) const {
    const wxSize sz = GetClientSize();
    dc.SetTextForeground(wxColour(150, 150, 150));
    dc.SetFont(wxFontInfo(11));

    const wxString line1 = m_placeType.empty()
                               ? U8("从左侧元件库选择元件 → 点击画布放置")
                               : U8("放置模式:") + wxString::FromUTF8(m_placeType.c_str()) +
                                     U8(" —— 点击画布放置;Esc 退出");
    const wxString line2 = U8("放好元件后:从引脚拖到引脚连线;双击开关拨动电平");
    wxSize s1 = dc.GetTextExtent(line1);
    wxSize s2 = dc.GetTextExtent(line2);
    dc.DrawText(line1, (sz.x - s1.x) / 2, sz.y / 2 - s1.y);
    dc.DrawText(line2, (sz.x - s2.x) / 2, sz.y / 2 + s2.y / 2);
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

    // 选中/悬停:蓝色虚/实线外框(先画,免得盖住元件体)
    if (selected || hovered) {
        dc.SetPen(wxPen(wxColour(0, 90, 200), selected ? 2 : 1,
                        selected ? wxPENSTYLE_SOLID : wxPENSTYLE_DOT));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(o.x - hw - 5, o.y - hh - 5, sz.w + 10, sz.h + 10);
    }

    // 元件体:矩形占位(v1);开关/ LED 在体内画状态
    const editor::SignalLevel lv =
        IsSwitch(c) ? SwitchLevel(c.id)
                    : (IsLed(c) ? PinLevel(editor::PinRef{c.id, 0})
                                : editor::SignalLevel::Undefined);

    wxBrush bodyBrush(*wxWHITE_BRUSH);
    if (IsSwitch(c) && lv == editor::SignalLevel::High) {
        bodyBrush = wxBrush(wxColour(212, 245, 212));    // 开关闭合:淡绿底
    }
    dc.SetPen(wxPen(wxColour(40, 40, 40), 2));
    dc.SetBrush(bodyBrush);
    dc.DrawRectangle(o.x - hw, o.y - hh, sz.w, sz.h);

    if (IsSwitch(c)) {                                   // 开关:1 / 0
        dc.SetTextForeground(lv == editor::SignalLevel::High ? wxColour(0, 140, 0)
                                                             : wxColour(130, 130, 130));
        dc.SetFont(wxFontInfo(10).Bold());
        const wxString t = (lv == editor::SignalLevel::High) ? "1" : "0";
        const wxSize ts = dc.GetTextExtent(t);
        dc.DrawText(t, o.x - ts.x / 2, o.y - ts.y / 2);
    } else if (IsLed(c)) {                               // LED:亮 = 实心橙圆
        const bool on = lv == editor::SignalLevel::High;
        dc.SetPen(wxPen(on ? wxColour(200, 110, 0) : wxColour(120, 120, 120), 2));
        dc.SetBrush(on ? wxBrush(wxColour(255, 170, 0))
                       : (lv == editor::SignalLevel::Undefined
                              ? wxBrush(wxColour(225, 225, 225))
                              : *wxWHITE_BRUSH));
        dc.DrawCircle(o.x, o.y, 9);
    }

    dc.SetFont(wxFontInfo(8));
    for (size_t i = 0; i < c.pins.size(); ++i) {
        const editor::PinDescriptor& pin = c.pins[i];
        const editor::Point abs{c.pos.x + pin.relPos.x, c.pos.y + pin.relPos.y};
        const wxPoint pe = ToScreen(abs);

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

    wxColour colour;
    int width = 2;
    switch (WireLevel(w)) {                              // 电平着色(Logisim 风格)
        case editor::SignalLevel::High:
            colour = wxColour(0, 180, 0);
            width = 3;
            break;
        case editor::SignalLevel::Low:
            colour = wxColour(0, 95, 0);
            break;
        default:                                         // 悬空 / 未初始化
            colour = wxColour(165, 165, 165);
            break;
    }
    if (m_sel.IsWire() && m_sel.id == w.id) {            // 选中的线盖过电平色
        colour = wxColour(0, 90, 200);
        width = 4;
    }

    dc.SetPen(wxPen(colour, width));
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

    editor::Point end = m_mouseLogical;
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
// 收尾辅助
// ---------------------------------------------------------------------------

void CanvasPanel::EndInteraction() {
    if (HasCapture()) {
        ReleaseMouse();
    }
    m_mode = Mode::Idle;
    m_dragId.clear();
    m_wireSnap = HitResult{};
    m_bandRect = wxRect(-1, -1, 0, 0);
}

void CanvasPanel::CancelInteraction(const wxString& status) {
    EndInteraction();
    SetStatus(status);
}

void CanvasPanel::SetStatus(const wxString& text) {
    if (m_status) {
        m_status(text);
    }
}

void CanvasPanel::NotifySelection() {
    if (!m_onSelection) {
        return;
    }
    const editor::Component* sel =
        (m_sel.IsComponent() || m_sel.IsPin()) ? Find(m_sel.id) : nullptr;
    m_onSelection(sel);
}

void CanvasPanel::NotifyChanged() {
    m_indexDirty = true;          // 元件可能增删,索引缓存下次访问时重建
    if (m_onChanged) {
        m_onChanged();
    }
}

// ---------------------------------------------------------------------------
// 视图平移(issue 1):坐标换算已经集中在 ToScreen/ToLogical,
// 所以平移只改 m_origin —— 绘制与命中检测都不需要改。
// ---------------------------------------------------------------------------

void CanvasPanel::PanBy(const wxPoint& deltaDevice) {
    m_origin += deltaDevice;
    ClampOrigin();
    Refresh();
}

wxRect CanvasPanel::WorldRectLogical() const {
    wxRect world;
    bool first = true;
    for (const auto& c : S().components) {
        const SymbolSize sz = SizeOf(c.type);
        const wxRect cr(c.pos.x - sz.w / 2 - 60, c.pos.y - sz.h / 2 - 40,
                        sz.w + 120, sz.h + 80);          // 含标签与引脚余量
        if (first) {
            world = cr;
            first = false;
        } else {
            world.Union(cr);
        }
    }
    if (first) {
        world = wxRect(0, 0, 1200, 800);                 // 空文档也给一片可平移的世界
    }
    return world;
}

void CanvasPanel::ClampOrigin() {
    const wxSize vp = GetClientSize();
    const wxRect world = WorldRectLogical();
    const int left = static_cast<int>(world.x * m_scale);
    const int top = static_cast<int>(world.y * m_scale);
    const int right = static_cast<int>((world.x + world.width) * m_scale);
    const int bottom = static_cast<int>((world.y + world.height) * m_scale);

    // 内容至少留 kPanKeep 像素在视口里,防止把原理图"推丢"
    const int minX = -right + kPanKeep;
    const int maxX = vp.x - left - kPanKeep;
    const int minY = -bottom + kPanKeep;
    const int maxY = vp.y - top - kPanKeep;

    m_origin.x = std::max(minX, std::min(maxX, m_origin.x));
    m_origin.y = std::max(minY, std::min(maxY, m_origin.y));
}

// ---------------------------------------------------------------------------
// 局部重绘(issue 4):拖拽 / 橡皮筋 / 悬停只刷新受影响的小矩形,
// 网格绘制也只在重绘区域内循环。
// ---------------------------------------------------------------------------

wxRect CanvasPanel::ScreenRectOf(const editor::Component& c, int margin) const {
    const SymbolSize sz = SizeOf(c.type);
    const wxPoint o = ToScreen(c.pos);
    return wxRect(o.x - sz.w / 2 - margin, o.y - sz.h / 2 - margin,
                  sz.w + 2 * margin, sz.h + 2 * margin);
}

wxRect CanvasPanel::ScreenRectOfWire(const editor::Wire& w, int margin) const {
    editor::Point a;
    editor::Point b;
    if (!PinRefLogicalPos(w.from, &a) || !PinRefLogicalPos(w.to, &b)) {
        return wxRect();
    }
    wxRect r(ToScreen(a), ToScreen(b));
    r.Inflate(margin);
    return r;
}

wxRect CanvasPanel::RubberBandScreenRect() const {
    if (m_mode != Mode::DrawingWire) {
        return wxRect();
    }
    editor::Point start;
    if (!PinRefLogicalPos(m_wireFrom, &start)) {
        return wxRect();
    }
    editor::Point end = m_mouseLogical;
    if (m_wireSnap.IsPin()) {
        editor::Point snapped;
        if (PinRefLogicalPos(editor::PinRef{m_wireSnap.id, m_wireSnap.pinIndex}, &snapped)) {
            end = snapped;
        }
    }
    wxRect r(ToScreen(start), ToScreen(end));
    r.Inflate(kWireHitR * 2);                            // 覆盖线宽与端点小圆
    return r;
}

void CanvasPanel::RefreshComponentArea(const std::string& id) {
    const editor::Component* c = Find(id);
    if (c == nullptr) {
        return;
    }
    wxRect r = ScreenRectOf(*c, kCmpMargin);
    for (const auto& w : S().wires) {                    // 与它相连的导线端点也会动
        if (w.from.componentId == id || w.to.componentId == id) {
            r.Union(ScreenRectOfWire(w, 6));
        }
    }
    if (!r.IsEmpty()) {
        RefreshRect(r);
    }
}

void CanvasPanel::RefreshRubberBandArea(const wxRect& previous) {
    if (!previous.IsEmpty()) {
        RefreshRect(previous);                           // 擦掉上一帧的橡皮筋
    }
    const wxRect now = RubberBandScreenRect();
    if (!now.IsEmpty()) {
        RefreshRect(now);
    }
}
