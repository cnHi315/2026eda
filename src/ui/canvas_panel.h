#pragma once
// 中间绘图区(负责人 B,任务 4)。
// 阶段一:仅作为占位容器;阶段二起在此实现网格、元件/导线绘制与坐标换算。

#include <wx/panel.h>

class CanvasPanel : public wxPanel {
public:
    explicit CanvasPanel(wxWindow* parent);
};
