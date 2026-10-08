#pragma once
// 右侧属性表面板(负责人 B,任务 2)。
// 阶段四:显示选中元件(findComponent 的结果),只读。

#include <wx/panel.h>

namespace editor {
struct Component;
} // namespace editor

class wxPropertyGrid;

class PropertyPanel : public wxPanel {
public:
    explicit PropertyPanel(wxWindow* parent);

    /// 显示选中元件(只读);传 nullptr = 当前没有选中元件。
    void ShowComponent(const editor::Component* c);

private:
    wxPropertyGrid* m_grid = nullptr;
};
