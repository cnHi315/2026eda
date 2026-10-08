#pragma once
// 中间绘图区(负责人 B,任务 2、任务 4)。
//
// 阶段二:只读画布渲染 —— 网格 + 元件 + 导线。
// 阶段三:交互与布线核心 —— 命中检测 / 元件拖拽 / 橡皮筋连线 / 选中高亮。
// 阶段四(真数据上屏):数据改读 SchematicModel::data(),所有写操作走 model
//   (addElement / moveElement / addWire / removeWire / removeElement);
//   双击 SWITCH 翻转电平 → Simulator::setInput + step,导线按电平着色、LED 亮灭。
//   开关状态**默认关闭**,只存在 UI 内存里(v1 不写进 JSON,打开文件后回到关闭)。
//
// 坐标系:遵循契约 —— int 逻辑坐标,原点在左上角,y 轴向下(与 wxDC 同向)。
//   HiDPI:wxWidgets 3.2 的 wxDC 与鼠标事件在缩放屏幕上已经是 DIP 单位
//   (本机 2.0 缩放时 20 逻辑像素 = 40 物理像素),所以逻辑坐标、命中半径、线宽
//   统一用同一套单位,**不能**再套 FromDIP() 二次放大。

#include <wx/dc.h>
#include <wx/panel.h>

#include <functional>
#include <string>
#include <unordered_map>

#include "contract/data_model.h"

namespace editor {
class SchematicModel;
class Simulator;
} // namespace editor

class CanvasPanel : public wxPanel {
public:
    /// 状态栏文本(选中 / 拖拽中 / 连线中 / 失败原因 / 放置模式)。
    using StatusCallback = std::function<void(const wxString&)>;
    /// 选中变化:传 nullptr 表示没有选中元件(选中导线或空白)。
    using SelectionCallback = std::function<void(const editor::Component*)>;
    /// 文档发生结构变化(放置 / 删除 / 连线 / 拨开关):通知外层刷新计数与属性表。
    using ChangedCallback = std::function<void()>;

    explicit CanvasPanel(wxWindow* parent);

    /// 接入后端(阶段四)。构造后必须调用一次。
    void Attach(editor::SchematicModel* model, editor::Simulator* sim);
    void SetStatusCallback(StatusCallback cb);
    void SetSelectionCallback(SelectionCallback cb);
    void SetChangedCallback(ChangedCallback cb);

    /// 放置模式:元件树选中类型后进入,画布左键点空白 → addElement;空串退出。
    void SetPlacementType(const std::string& type);
    void ClearPlacementType();
    const std::string& PlacementType() const { return m_placeType; }

    /// 删除当前选中的元件 / 导线(Delete 键与"编辑 → 删除"共用)。
    bool DeleteSelection();

    /// 文档被替换(新建 / 打开)之后调用:清选中、清开关状态、重建仿真。
    void OnDocumentReplaced();

    /// 结构变化后重建仿真模型并重放开关状态。
    void ResyncSimulation();

private:
    // —— 命中结果 ——
    enum class HitKind { None, Component, Pin, Wire };

    struct HitResult {
        HitKind kind = HitKind::None;
        std::string id;        ///< 元件 id(Component/Pin)或导线 id(Wire)
        int pinIndex = -1;     ///< 仅 Pin 有效

        bool IsComponent() const { return kind == HitKind::Component; }
        bool IsPin() const { return kind == HitKind::Pin; }
        bool IsWire() const { return kind == HitKind::Wire; }
        bool IsNone() const { return kind == HitKind::None; }
        bool SameAs(const HitResult& o) const {
            return kind == o.kind && id == o.id && pinIndex == o.pinIndex;
        }
    };

    // —— 交互状态机 ——
    enum class Mode { Idle, DraggingComponent, DrawingWire };

    // —— 事件 ——
    void OnPaint(wxPaintEvent& evt);
    void OnSize(wxSizeEvent& evt);
    void OnLeftDown(wxMouseEvent& evt);
    void OnLeftDClick(wxMouseEvent& evt);
    void OnMotion(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnLeaveWindow(wxMouseEvent& evt);
    void OnCaptureLost(wxMouseCaptureLostEvent& evt);
    void OnKeyDown(wxKeyEvent& evt);

    // —— 绘制 ——
    void DrawGrid(wxDC& dc) const;
    void DrawEmptyHint(wxDC& dc) const;
    void DrawComponent(wxDC& dc, const editor::Component& c) const;
    void DrawWire(wxDC& dc, const editor::Wire& w) const;
    void DrawPinDot(wxDC& dc, const wxPoint& p, bool highlight) const;
    void DrawRubberBand(wxDC& dc) const;

    // —— 坐标换算(逻辑坐标 <-> 屏幕像素) ——
    wxPoint ToScreen(const editor::Point& p) const;
    editor::Point ToLogical(const wxPoint& p) const;

    // —— 命中检测(优先级:引脚 → 元件 → 导线) ——
    HitResult HitTest(const editor::Point& logical) const;
    HitResult HitTestPin(const editor::Point& logical) const;
    HitResult HitTestComponent(const editor::Point& logical) const;
    HitResult HitTestWire(const editor::Point& logical) const;

    // —— 几何辅助 ——
    struct SymbolSize { int w; int h; int pinLen; };
    static SymbolSize SizeOf(const std::string& type);
    const editor::Component* Find(const std::string& id) const;
    const editor::Schematic& S() const;
    bool PinRefLogicalPos(const editor::PinRef& pin, editor::Point* out) const;
    static editor::Point SnapToGrid(const editor::Point& p);
    static bool IsOutputPin(const editor::Component& c, int pinIndex);
    static bool IsSwitch(const editor::Component& c);
    static bool IsLed(const editor::Component& c);

    // —— 写操作(全部走 model) ——
    bool PlaceAt(const editor::Point& logical);
    void MoveComponentTo(const std::string& id, const editor::Point& pos);
    bool CanConnect(const editor::PinRef& from, const editor::PinRef& to,
                    wxString* reason) const;
    void AddWire(const editor::PinRef& from, const editor::PinRef& to);
    void ToggleSwitch(const std::string& id);

    // —— 仿真辅助 ——
    editor::SignalLevel SwitchLevel(const std::string& id) const;
    void ApplySwitchInputs();
    editor::SignalLevel PinLevel(const editor::PinRef& pin) const;
    editor::SignalLevel WireLevel(const editor::Wire& w) const;

    void EndInteraction();
    void CancelInteraction(const wxString& status);
    void SetStatus(const wxString& text);
    void NotifySelection();
    void NotifyChanged();

    editor::SchematicModel* m_model = nullptr;   ///< 不持有,由 MainFrame 管理
    editor::Simulator* m_sim = nullptr;          ///< 不持有
    double m_scale = 1.0;                        ///< 预留缩放;当前恒为 1.0
    wxPoint m_origin{0, 0};                      ///< 逻辑原点的屏幕偏移(预留平移)

    // —— 交互状态 ——
    Mode m_mode = Mode::Idle;
    HitResult m_sel;
    HitResult m_hover;
    std::string m_placeType;             ///< 非空 = 放置模式(元件树选中)
    std::string m_dragId;
    editor::Point m_dragGrab{0, 0};
    editor::Point m_dragOrig{0, 0};
    bool m_dragMoved = false;
    editor::PinRef m_wireFrom;
    editor::Point m_mouseLogical{0, 0};
    HitResult m_wireSnap;

    /// 开关状态(默认不在表里 = 关闭 / Low);v1 不持久化。
    std::unordered_map<std::string, editor::SignalLevel> m_switchLevel;

    StatusCallback m_status;
    SelectionCallback m_onSelection;
    ChangedCallback m_onChanged;

    // —— 尺寸与命中容差(逻辑坐标;见 docs/interfaces.md 的几何参数表) ——
    static constexpr int kGridStep  = 20;
    static constexpr int kGridMajor = 5;
    static constexpr int kSymW      = 60;
    static constexpr int kSymH      = 40;
    static constexpr int kPinLen    = 20;
    static constexpr int kPinHitR   = 8;
    static constexpr int kWireHitR  = 4;
};
