#pragma once
// 左侧元件库面板(负责人 B,任务 2)。
// 阶段一:树里写死五类元件;阶段四改为由 ComponentLibrary::types() 填充。

#include <wx/panel.h>

class wxTreeCtrl;

class ComponentPalette : public wxPanel {
public:
    explicit ComponentPalette(wxWindow* parent);

    /// 阶段一占位数据:AND / OR / NOT / SWITCH / LED
    void FillPlaceholder();

private:
    wxTreeCtrl* m_tree = nullptr;
};
