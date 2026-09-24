#pragma once
// 中间绘图区(负责人 B,任务 4)。
//
// 阶段二:只读画布渲染 —— 网格背景 + 假元件 + 假导线。
//   数据自给自足(手填 editor::Schematic),不依赖 SchematicModel /
//   ComponentLibrary 的实现,只依赖契约 src/contract/data_model.h。
// 阶段四:移除 BuildDemoSchematic(),数据改读 SchematicModel::data()。
//
// 坐标系:遵循契约 —— int 逻辑坐标,原点在左上角,y 轴向下(与 wxDC 同向)。
// 逻辑坐标 <-> 屏幕坐标的换算集中在 ToScreen/ToLogical,阶段三命中检测复用。

#include <wx/dc.h>
#include <wx/panel.h>

#include <string>

#include "contract/data_model.h"

class CanvasPanel : public wxPanel {
public:
    explicit CanvasPanel(wxWindow* parent);

private:
    // —— 事件 ——
    void OnPaint(wxPaintEvent& evt);
    void OnSize(wxSizeEvent& evt);

    // —— 绘制 ——
    void DrawGrid(wxDC& dc) const;
    void DrawComponent(wxDC& dc, const editor::Component& c) const;
    void DrawWire(wxDC& dc, const editor::Wire& w) const;

    // —— 坐标换算(阶段三命中检测复用) ——
    wxPoint ToScreen(const editor::Point& p) const;
    editor::Point ToLogical(const wxPoint& p) const;

    // —— 阶段二假数据;阶段四删除,改读 SchematicModel ——
    static editor::Schematic BuildDemoSchematic();
    static const editor::Component* FindComponent(const editor::Schematic& s,
                                                  const std::string& id);

    editor::Schematic m_demo;      ///< 阶段四替换为 const SchematicModel&
    double m_scale = 1.0;          ///< 预留缩放;阶段二恒为 1.0
    wxPoint m_origin{0, 0};        ///< 逻辑原点的屏幕偏移(预留平移)

    // —— 网格与符号的临时尺寸(阶段四与 C 的 pinTemplate().relPos 对齐) ——
    static constexpr int kGridStep = 20;   ///< 细网格步长(逻辑坐标)
    static constexpr int kGridMajor = 5;   ///< 每 5 格一条粗线
    static constexpr int kSymW = 60;       ///< 元件矩形宽
    static constexpr int kSymH = 40;       ///< 元件矩形高
    static constexpr int kPinLen = 20;     ///< 引脚短线长度
};
