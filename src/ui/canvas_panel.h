#pragma once
// 中间绘图区(负责人 B,任务 2、任务 4)。
//
// 阶段二:只读画布渲染 —— 网格背景 + 假元件 + 假导线。
// 阶段三:交互与布线核心 —— 命中检测 / 元件拖拽 / 橡皮筋连线 / 选中高亮。
//   数据仍自给自足(手填 editor::Schematic),只依赖契约 src/contract/data_model.h,
//   不依赖 SchematicModel / ComponentLibrary 的实现。
// 阶段四:移除 BuildDemoSchematic(),数据改读 SchematicModel::data();
//   三个写入口(MoveComponentTo / CanConnect / AddWire)换成 model.moveElement / model.addWire。
//
// 坐标系:遵循契约 —— int 逻辑坐标,原点在左上角,y 轴向下(与 wxDC 同向)。
//   HiDPI 说明:wxWidgets 3.2 的 wxDC 与鼠标事件在缩放屏幕上已经是 DIP 单位
//   (本机 2.0 缩放时 20 逻辑像素 = 40 物理像素),所以逻辑坐标、命中半径、线宽
//   统一用同一套单位,**不能**再套 FromDIP() 二次放大。

#include <wx/dc.h>
#include <wx/panel.h>

#include <functional>
#include <string>

#include "contract/data_model.h"

class CanvasPanel : public wxPanel {
public:
    /// 状态栏联动:由 MainFrame 注入,把"选中 / 拖拽中 / 连线中"写进状态栏。
    using StatusCallback = std::function<void(const wxString&)>;

    explicit CanvasPanel(wxWindow* parent);
    void SetStatusCallback(StatusCallback cb);

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
    void OnMotion(wxMouseEvent& evt);
    void OnLeftUp(wxMouseEvent& evt);
    void OnLeaveWindow(wxMouseEvent& evt);
    void OnCaptureLost(wxMouseCaptureLostEvent& evt);
    void OnKeyDown(wxKeyEvent& evt);

    // —— 绘制 ——
    void DrawGrid(wxDC& dc) const;
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
    bool PinRefLogicalPos(const editor::PinRef& pin, editor::Point* out) const;
    static editor::Point SnapToGrid(const editor::Point& p);
    static bool IsOutputPin(const editor::Component& c, int pinIndex);

    // —— 数据写入(阶段四换成 SchematicModel 的调用) ——
    void MoveComponentTo(const std::string& id, const editor::Point& pos);
    bool CanConnect(const editor::PinRef& from, const editor::PinRef& to,
                    wxString* reason) const;
    void AddWire(const editor::PinRef& from, const editor::PinRef& to);
    void EndInteraction();
    void CancelInteraction(const wxString& status);
    void SetStatus(const wxString& text);

    // —— 阶段二假数据;阶段四删除,改读 SchematicModel ——
    static editor::Schematic BuildDemoSchematic();
    static const editor::Component* FindComponent(const editor::Schematic& s,
                                                  const std::string& id);

    editor::Schematic m_demo;      ///< 阶段四替换为 const SchematicModel&
    double m_scale = 1.0;          ///< 预留缩放;阶段三恒为 1.0
    wxPoint m_origin{0, 0};        ///< 逻辑原点的屏幕偏移(预留平移)

    // —— 交互状态 ——
    Mode m_mode = Mode::Idle;
    HitResult m_sel;                    ///< 选中:元件 / 引脚 / 导线
    HitResult m_hover;                  ///< 悬停:仅用于引脚高亮
    std::string m_dragId;               ///< 拖拽中的元件 id
    editor::Point m_dragGrab{0, 0};     ///< 抓取偏移(鼠标 - 元件原点)
    editor::Point m_dragOrig{0, 0};     ///< 拖拽前的位置(Esc 可还原)
    bool m_dragMoved = false;           ///< 是否真的移动过(区分"单击选中"与"拖拽")
    editor::PinRef m_wireFrom;          ///< 橡皮筋起点引脚
    editor::Point m_mouseLogical{0, 0}; ///< 鼠标当前位置(逻辑坐标)
    HitResult m_wireSnap;               ///< 橡皮筋吸附到的目标引脚
    StatusCallback m_status;

    // —— 尺寸与命中容差(全部为逻辑坐标单位) ——
    static constexpr int kGridStep  = 20;  ///< 细网格步长,同时是拖拽吸附步长
    static constexpr int kGridMajor = 5;   ///< 每 5 格一条粗线
    static constexpr int kSymW      = 60;  ///< 元件包围盒宽(±50 = kSymW/2 + kPinLen)
    static constexpr int kSymH      = 40;  ///< 元件包围盒高
    static constexpr int kPinLen    = 20;  ///< 引脚短线长度
    static constexpr int kPinHitR   = 8;   ///< 引脚命中半径
    static constexpr int kWireHitR  = 4;   ///< 导线命中容差
};
