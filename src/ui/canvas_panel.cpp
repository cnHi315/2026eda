#include "ui/canvas_panel.h"

#include <wx/wx.h>
#include <wx/dcbuffer.h>

#include <algorithm>
#include <cmath>
#include <string>

#include "model/schematic_model.h"
#include "simulation/simulator.h"
#include "ui/subcircuit_library.h"

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
    Bind(wxEVT_DPI_CHANGED, [this](wxDPIChangedEvent& event) {
        m_textSizes.clear(); Refresh(); event.Skip();
    });
}

void CanvasPanel::Attach(editor::SchematicModel* model, editor::Simulator* sim) {
    m_model = model;
    m_sim = sim;
}

void CanvasPanel::SetStatusCallback(StatusCallback cb) { m_status = std::move(cb); }
void CanvasPanel::SetSelectionCallback(SelectionCallback cb) { m_onSelection = std::move(cb); }
void CanvasPanel::SetChangedCallback(ChangedCallback cb) { m_onChanged = std::move(cb); }
void CanvasPanel::SetPlacementCallback(PlacementCallback cb) { m_onPlacement = std::move(cb); }

void CanvasPanel::SetPlacementType(const std::string& type) {
    const bool changed = (m_placeType != type) || m_placeFragment;
    m_placeFragment = false;
    m_fragment = {};
    m_placeType = type;
    if (!type.empty()) {
        SetStatus(U8("放置模式:") + wxString::FromUTF8(type.c_str()) +
                  U8(" —— 点击画布空白处放置;Esc 退出"));
    }
    if (changed && m_onPlacement) {
        m_onPlacement(m_placeType);
    }
    Refresh();
}

void CanvasPanel::ClearPlacementType() {
    if (!m_placeType.empty() || m_placeFragment) {
        m_placeType.clear();
        m_placeFragment = false;
        m_fragment = {};
        SetStatus(U8("已退出放置模式"));
        if (m_onPlacement) {
            m_onPlacement(std::string{});        // 同步左侧元件树的高亮
        }
        Refresh();
    }
}

bool CanvasPanel::SetSubcircuitPlacement(const editor::Schematic& fragment, const std::string& name) {
    wxString error;
    if (!SubcircuitLibrary::Validate(fragment, &error)) { SetStatus(error); return false; }
    if (m_mode != Mode::Idle) return false;
    m_placeType.clear();
    m_fragment = fragment;
    m_fragmentName = name;
    m_placeFragment = true;
    if (m_onPlacement) m_onPlacement("subcircuit:" + name);
    SetStatus(U8("放置子电路：") + wxString::FromUTF8(name.c_str()) + U8("；点击空白处放置，Esc 退出"));
    return true;
}

bool CanvasPanel::ExportSelection(editor::Schematic* fragment, wxString* error) const {
    if (!fragment) return false;
    *fragment = {};
    if (m_mode != Mode::Idle || m_selectedIds.empty()) {
        if (error) *error = U8("请先结束当前操作并选择要封存的元件");
        return false;
    }
    for (const auto& c : S().components) if (m_selectedIds.count(c.id)) fragment->components.push_back(c);
    for (const auto& w : S().wires)
        if (m_selectedIds.count(w.from.componentId) && m_selectedIds.count(w.to.componentId)) fragment->wires.push_back(w);
    if (fragment->components.empty()) return false;
    int minX = fragment->components.front().pos.x, minY = fragment->components.front().pos.y;
    for (const auto& c : fragment->components) { minX = std::min(minX, c.pos.x); minY = std::min(minY, c.pos.y); }
    for (auto& c : fragment->components) { c.pos.x -= minX; c.pos.y -= minY; }
    editor::SchematicModel normalized;
    normalized.loadFrom(*fragment);
    *fragment = normalized.data();
    return SubcircuitLibrary::Validate(*fragment, error);
}

void CanvasPanel::OnDocumentReplaced(bool clearSwitchLevels) {
    EndInteraction();
    m_sel = HitResult{};
    m_hover = HitResult{};
    m_selectedIds.clear();
    if (clearSwitchLevels) {
        m_switchLevel.clear(); // 新建/打开归零；历史恢复保留挡位。
        m_textSizes.clear();
    }
    if (!m_placeType.empty() || m_placeFragment) {     // 同步元件树与子电路放置状态
        m_placeType.clear();        // 用户以为还在放置模式,点画布却没反应)
        m_placeFragment = false;
        m_fragment = {};
        if (m_onPlacement) {
            m_onPlacement(std::string{});
        }
    }
    ResyncSimulation();
    NotifySelection();
    NotifyChanged();

    // 撤销 / 重做 / 新建 / 打开 都由菜单命令(或菜单命令后面跟着模态对话框)触发。
    // 某些平台下此时 Refresh() 只是把窗口标脏,真正出帧要等下一条外部消息(鼠标移动、
    // 焦点变化…)才发生 —— 表现就是"按了 Ctrl+Z 画面不动,直到动一下鼠标才变"。
    // 这里 Update() 立即出一帧,再 CallAfter 补一次,确保与调用时序无关。
    Refresh();
    Update();
    CallAfter([this]() {
        Refresh();
        Update();
    });
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

    wxRect clip;
    if (!dc.GetClippingBox(clip) || clip.IsEmpty()) clip = GetClientRect();
    clip.Intersect(GetClientRect());
    DrawGrid(dc);                                        // ① 网格在最底层
    for (const auto& w : S().wires) {                    // ② 导线
        if (ScreenRectOfWire(w, 6).Intersects(clip)) DrawWire(dc, w);
    }
    for (const auto& c : S().components) {               // ③ 元件压在导线之上
        // Include labels/pins in the bounds, not just the symbol body.
        if (ScreenRectOf(c, kCmpMargin).Intersects(clip)) DrawComponent(dc, c);
    }
    DrawRubberBand(dc);                                  // ④ 连线预览在最上层
    DrawSelectionBox(dc);

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

    if (evt.ControlDown() && (hit.IsComponent() || hit.IsPin())) {
        if (m_selectedIds.count(hit.id)) m_selectedIds.erase(hit.id);
        else m_selectedIds.insert(hit.id);
        UpdateSelectionFocus();
        SetStatus(wxString::Format(U8("已选中 %zu 个元件"), m_selectedIds.size()));
        NotifySelection(); Refresh(); return;
    }

    if (hit.IsPin()) {                                   // —— 从引脚起一条线 ——
        m_selectedIds.clear();
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
        m_dragBefore = S();                              // 按下时取快照(抬起确认移动过才入栈)
        m_dragGrab = editor::Point{p.x - c->pos.x, p.y - c->pos.y};
        m_dragMoved = false;
        if (!m_selectedIds.count(hit.id)) m_selectedIds = {hit.id};
        m_dragPositions.clear();
        for (const auto& c : S().components) if (m_selectedIds.count(c.id)) m_dragPositions[c.id] = c.pos;
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
        m_selectedIds.clear();
        m_sel = hit;
        SetStatus(U8("选中导线 ") + wxString::FromUTF8(hit.id.c_str()));
        NotifySelection();
        Refresh();
        return;
    }

    if (m_placeFragment) { PlaceFragmentAt(p); return; }
    if (!m_placeType.empty()) {                          // —— 放置元件 ——
        PlaceAt(p);
        return;
    }

    m_mode = Mode::SelectingBox;
    m_boxStart = m_boxEnd = p;
    m_boxBefore = m_selectedIds;
    if (!evt.ControlDown()) m_selectedIds.clear();
    UpdateSelectionFocus();
    if (!HasCapture()) CaptureMouse();
    SetStatus(U8("框选元件 —— 拖动空白区域，Esc 取消"));
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

    if (m_mode == Mode::SelectingBox) {
        m_boxEnd = p;
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

        wxRect oldArea = DragArea();
        MoveDraggedTo(SnapToGrid(raw));
        if (!oldArea.IsEmpty()) {
            RefreshRect(oldArea);
        }
        const auto newArea = DragArea();
        if (!newArea.IsEmpty()) RefreshRect(newArea);
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

    if (m_mode == Mode::SelectingBox) {
        m_boxEnd = p;
        const wxRect box = SelectionBox();
        if (box.width > 3 || box.height > 3) {
            for (const auto& c : S().components) {
                const auto size = SizeOf(c.type);
                if (box.Contains(wxRect(c.pos.x - size.w/2, c.pos.y - size.h/2, size.w, size.h)))
                    m_selectedIds.insert(c.id);
            }
        }
        UpdateSelectionFocus();
        EndInteraction();
        SetStatus(m_selectedIds.empty() ? U8("就绪 —— 拖动元件,或从引脚拖出连线")
                                      : wxString::Format(U8("已选中 %zu 个元件"), m_selectedIds.size()));
        NotifySelection(); Refresh(); return;
    }

    if (m_mode == Mode::DraggingComponent) {
        const std::string id = m_dragId;
        if (m_dragMoved) {
            const editor::Point target =
                SnapToGrid(editor::Point{p.x - m_dragGrab.x, p.y - m_dragGrab.y});
            // 实时位置已改变；整组确认落点后，按按下时的快照只提交一次历史。
            if (target.x != m_dragOrig.x || target.y != m_dragOrig.y) {
                MoveDraggedTo(target);
                PushUndoSnapshot(m_dragBefore);          // 按下时的快照
                NotifyChanged();
            }
            SetStatus(U8("放下 ") + wxString::FromUTF8(id.c_str()) + U8(" 位置:(") +
                      wxString::Format("%d,%d", target.x, target.y) + U8(")"));
        } else {
            MoveDraggedTo(m_dragOrig);  // 单击:只选中,不入栈
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
        if (m_mode == Mode::DraggingComponent) MoveDraggedTo(m_dragOrig);
        if (m_mode == Mode::SelectingBox) { m_selectedIds = m_boxBefore; UpdateSelectionFocus(); NotifySelection(); }
        CancelInteraction(U8("操作已取消"));
        Refresh();
    }
}

void CanvasPanel::OnMiddleDown(wxMouseEvent& evt) {
    if (m_mode != Mode::Idle) return;
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
            MoveDraggedTo(m_dragOrig);  // Esc:还原整组,不入栈
            CancelInteraction(U8("已取消拖动(位置已还原)"));
            Refresh();
            return;
        }
        if (m_mode == Mode::SelectingBox) {
            m_selectedIds = m_boxBefore;
            UpdateSelectionFocus();
            CancelInteraction(U8("已取消框选"));
            NotifySelection(); Refresh(); return;
        }
        if (m_mode == Mode::DrawingWire) {
            CancelInteraction(U8("已取消连线"));
            Refresh();
            return;
        }
        if (!m_placeType.empty() || m_placeFragment) {
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
    const editor::Schematic before = S();                 // 失败不入栈,先留一份旧数据
    const std::string id = m_model->addElement(m_placeType, pos);
    if (id.empty()) {
        SetStatus(U8("放置失败:未知元件类型 ") + wxString::FromUTF8(m_placeType.c_str()));
        return false;
    }

    PushUndoSnapshot(before);
    ResyncSimulation();
    HitResult sel;
    sel.kind = HitKind::Component;
    sel.id = id;
    m_sel = sel;
    m_selectedIds = {id};
    SetStatus(U8("已放置 ") + wxString::FromUTF8(id.c_str()) + U8("(") +
              wxString::FromUTF8(m_placeType.c_str()) + U8(") 位置:(") +
              wxString::Format("%d,%d", pos.x, pos.y) + U8(")"));
    NotifySelection();
    NotifyChanged();
    Refresh();
    return true;
}

bool CanvasPanel::PlaceFragmentAt(const editor::Point& logical) {
    if (!m_model || !m_placeFragment) return false;
    const auto before = S();
    const auto created = m_model->instantiate(m_fragment, SnapToGrid(logical));
    if (created.size() != m_fragment.components.size()) {
        m_model->loadFrom(before);
        m_indexDirty = true;
        SetStatus(U8("放置子电路失败，图纸保持不变"));
        return false;
    }
    // instantiate assigns new IDs; keep explicitly named components readable.
    auto placed = S();
    for (size_t i = 0; i < created.size(); ++i) {
        const auto& source = m_fragment.components[i];
        for (auto& c : placed.components) if (c.id == created[i] && !source.name.empty() && source.name != source.id)
            c.name = source.name;
    }
    m_model->loadFrom(placed);
    PushUndoSnapshot(before);
    m_selectedIds = std::set<std::string>(created.begin(), created.end());
    UpdateSelectionFocus();
    NotifyChanged(); ResyncSimulation(); NotifySelection();
    SetStatus(wxString::Format(U8("已放置子电路，选中 %zu 个元件"), created.size()));
    Refresh();
    return true;
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
    const editor::Schematic before = S();
    const std::string netId = m_model->addWire(from, to);
    if (netId.empty()) {
        SetStatus(U8("连线未创建:模型拒绝了这条连线"));
        return;
    }

    PushUndoSnapshot(before);
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
    if (m_model == nullptr || m_mode != Mode::Idle) {
        return false;
    }
    if (!m_selectedIds.empty() || m_sel.IsComponent() || m_sel.IsPin()) {
        const auto selected = m_selectedIds.empty() ? std::set<std::string>{m_sel.id} : m_selectedIds;
        const editor::Schematic before = S();
        size_t removed = 0;
        for (const auto& id : selected) {
            if (m_model->removeElement(id)) ++removed;
        }
        if (removed) {
            PushUndoSnapshot(before);
            for (const auto& id : selected) m_switchLevel.erase(id);
            m_sel = HitResult{};
            m_selectedIds.clear();
            ResyncSimulation();
            SetStatus(wxString::Format(U8("已删除 %zu 个元件"), removed));
            NotifySelection();
            NotifyChanged();
            Refresh();
            return true;
        }
        SetStatus(U8("删除失败：选中的元件不存在"));
        return false;
    }
    if (m_sel.IsWire()) {
        const std::string id = m_sel.id;
        const editor::Schematic before = S();
        if (m_model->removeWire(id)) {
            PushUndoSnapshot(before);
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

    const wxString line1 = m_placeFragment
                               ? U8("放置子电路：") + wxString::FromUTF8(m_fragmentName.c_str()) + U8(" —— 点击空白处放置；Esc 退出")
                               : m_placeType.empty()
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
    const bool selected = m_selectedIds.count(c.id) != 0 || (m_sel.IsComponent() && m_sel.id == c.id);
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
        dc.DrawText(U8(pin.name.c_str()), pe.x + kPinLabelDx, pe.y - kPinLabelDy);
    }

    const std::string& label = c.name.empty() ? c.id : c.name;
    dc.SetTextForeground(wxColour(30, 30, 30));
    dc.DrawText(U8(label.c_str()), o.x - hw, o.y - hh - kCmpNameDy);
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
    m_dragPositions.clear();
    m_wireSnap = HitResult{};
    m_bandRect = wxRect(-1, -1, 0, 0);
}

void CanvasPanel::UpdateSelectionFocus() {
    m_sel = {};
    if (m_selectedIds.size() == 1) {
        m_sel.kind = HitKind::Component;
        m_sel.id = *m_selectedIds.begin();
    }
}

wxRect CanvasPanel::SelectionBox() const {
    return wxRect(std::min(m_boxStart.x, m_boxEnd.x), std::min(m_boxStart.y, m_boxEnd.y),
                  std::abs(m_boxEnd.x - m_boxStart.x) + 1, std::abs(m_boxEnd.y - m_boxStart.y) + 1);
}

void CanvasPanel::DrawSelectionBox(wxDC& dc) const {
    if (m_mode != Mode::SelectingBox) return;
    const auto box = SelectionBox();
    const auto start = ToScreen({box.x, box.y});
    dc.SetPen(wxPen(wxColour(0, 90, 200), 1, wxPENSTYLE_SHORT_DASH));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(start.x, start.y, int(box.width * m_scale), int(box.height * m_scale));
}

void CanvasPanel::MoveDraggedTo(const editor::Point& position) {
    if (!m_model) return;
    bool changed = false;
    for (const auto& entry : m_dragPositions) {
        const editor::Point target{entry.second.x + position.x - m_dragOrig.x,
                                   entry.second.y + position.y - m_dragOrig.y};
        const auto* current = m_model->findComponent(entry.first);
        if (current && (current->pos.x != target.x || current->pos.y != target.y))
            changed = m_model->moveElement(entry.first, target) || changed;
    }
    if (changed) NotifyChanged();
}

wxRect CanvasPanel::DragArea() const {
    wxRect area;
    for (const auto& entry : m_dragPositions) {
        if (const auto* c = Find(entry.first)) area.Union(ScreenRectOf(*c, kCmpMargin));
    }
    for (const auto& w : S().wires) {
        if (m_dragPositions.count(w.from.componentId) || m_dragPositions.count(w.to.componentId))
            area.Union(ScreenRectOfWire(w, 6));
    }
    return area;
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
// 撤销 / 重做(F-06,快照式)
//
// 设计(第 5 周定稿):
//   · 快照 = 整份 editor::Schematic(model.data());靠 loadFrom() 恢复,
//     nets 会按 wires 重算,所以快照天然自洽,不需要写反向操作;
//   · 只记"真正的编辑":放置 / 拖动(抬起且确实移动过)/ 连线 / 删除元件与导线。
//     单击选中、Esc 还原、拨开关(只改仿真状态,不改原理图)都不入栈;
//   · 任何新编辑都会清空重做栈;栈深上限 kUndoDepth,超出丢最旧;
//   · 历史恢复保留仍存在开关的当前挡位；恢复被删除开关时使用快照中的挡位。
//     新建/打开归零；拨开关本身仍不单独入栈。
// ---------------------------------------------------------------------------

void CanvasPanel::PushUndoSnapshot(const editor::Schematic& before) {
    m_undo.push_back(CaptureHistory(before));
    while (m_undo.size() > kUndoDepth) {
        m_undo.pop_front();
    }
    m_redo.clear();                                      // 新编辑使"重做"失效
}

CanvasPanel::HistorySnapshot CanvasPanel::CaptureHistory(const editor::Schematic& schematic) const {
    HistorySnapshot snapshot;
    snapshot.schematic = schematic;
    for (const auto& c : schematic.components)
        if (IsSwitch(c)) snapshot.switches[c.id] = SwitchLevel(c.id);
    return snapshot;
}

void CanvasPanel::RestoreHistory(const HistorySnapshot& snapshot) {
    std::unordered_map<std::string, editor::SignalLevel> levels;
    for (const auto& c : snapshot.schematic.components) {
        if (!IsSwitch(c)) continue;
        const auto* existing = m_model->findComponent(c.id);
        const auto saved = snapshot.switches.find(c.id);
        levels[c.id] = existing && IsSwitch(*existing) ? SwitchLevel(c.id)
            : saved != snapshot.switches.end() ? saved->second : editor::SignalLevel::Low;
    }
    m_model->loadFrom(snapshot.schematic);
    m_switchLevel = std::move(levels);
    OnDocumentReplaced(false);
}

void CanvasPanel::ClearHistory() {
    m_undo.clear();
    m_redo.clear();
}

bool CanvasPanel::Undo() {
    if (m_model == nullptr || m_undo.empty()) {
        return false;
    }
    if (m_mode != Mode::Idle) {
        // 拖动 / 连线还没落定:模型里已经有"未入栈"的中间状态。此时若真去撤销,
        // 撤掉的是上一条历史(比如刚放下的元件),看起来就像"撤销把放置和移动一起吃掉了"。
        // 约定:Ctrl+Z 在交互进行中不生效,先松手。
        SetStatus(U8("正在拖动或连线 —— 松开鼠标后再撤销"));
        return true;                                     // 已处理(只是不执行),不额外报错
    }
    m_redo.push_back(CaptureHistory(S()));
    const auto snapshot = m_undo.back();
    m_undo.pop_back();

    RestoreHistory(snapshot);
    SetStatus(U8("已撤销（保留现有开关状态）"));
    return true;
}

bool CanvasPanel::Redo() {
    if (m_model == nullptr || m_redo.empty()) {
        return false;
    }
    if (m_mode != Mode::Idle) {
        SetStatus(U8("正在拖动或连线 —— 松开鼠标后再重做"));
        return true;
    }
    m_undo.push_back(CaptureHistory(S()));
    const auto snapshot = m_redo.back();
    m_redo.pop_back();

    RestoreHistory(snapshot);
    SetStatus(U8("已重做（保留现有开关状态）"));
    return true;
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
    const int hw = sz.w / 2;
    const int hh = sz.h / 2;

    // 元件体 + 选中/悬停外框 + 命中余量
    wxRect r(o.x - hw - margin, o.y - hh - margin,
             sz.w + 2 * margin, sz.h + 2 * margin);

    // 元件名:画在符号框左上角再往上 kCmpNameDy,宽度可能超过符号框
    const auto nameSize = TextSize8(c.name.empty() ? c.id : c.name);
    r.Union(wxRect(o.x - hw,
                   o.y - hh - kCmpNameDy - std::max(kCmpNameH, nameSize.y),
                   std::max(kCmpNameW, nameSize.x + 2),
                   kCmpNameDy + std::max(kCmpNameH, nameSize.y) + 2));

    // 引脚:短线、端点圆点、以及画在端点右上方(框外)的引脚名。
    // 这一支就是"拖拽残留 Y"的修复点:kCmpMargin 够不到引脚名的右边缘。
    for (const auto& pin : c.pins) {
        const wxPoint pe = ToScreen(editor::Point{c.pos.x + pin.relPos.x,
                                                  c.pos.y + pin.relPos.y});
        const auto labelSize = TextSize8(pin.name);
        const int left = kPinHitR + 1;
        const int right = std::max(kPinLabelDx + std::max(kPinLabelW, labelSize.x + 2), kPinHitR + 1);
        const int up = kPinLabelDy + std::max(kPinLabelH, labelSize.y);
        const int down = std::max(kPinHitR + 1, labelSize.y - kPinLabelDy + 2);
        r.Union(wxRect(pe.x - left, pe.y - up, left + right, up + down));
    }
    return r;
}

wxSize CanvasPanel::TextSize8(const std::string& text) const {
    const auto cached = m_textSizes.find(text);
    if (cached != m_textSizes.end()) return cached->second;
    wxClientDC dc(const_cast<CanvasPanel*>(this));
    dc.SetFont(wxFontInfo(8)); // Same font as labels in DrawComponent.
    const auto size = dc.GetTextExtent(wxString::FromUTF8(text.c_str()));
    m_textSizes.emplace(text, size);
    return size;
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
