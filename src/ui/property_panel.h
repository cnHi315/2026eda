#pragma once
// 右侧属性表面板(负责人 B,任务 2)。
// 阶段一:显示占位属性行;阶段四接入 SchematicModel::findComponent()。

#include <wx/panel.h>

class wxPropertyGrid;

class PropertyPanel : public wxPanel {
public:
    explicit PropertyPanel(wxWindow* parent);

    /// 阶段一占位属性:id / 类型 / 名称 / 位置 / 旋转
    void FillPlaceholder();

private:
    wxPropertyGrid* m_grid = nullptr;
};
