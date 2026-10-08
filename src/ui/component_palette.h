#pragma once
// 左侧元件库面板(负责人 B,任务 2)。
// 阶段四:类型与显示名来自 ComponentLibrary(types() / displayName()),
//         选中某一项即通知画布进入"放置模式"。

#include <wx/panel.h>

#include <functional>
#include <string>

namespace editor {
class ComponentLibrary;
} // namespace editor

class wxTreeCtrl;
class wxTreeEvent;

class ComponentPalette : public wxPanel {
public:
    /// 选中某个元件类型(树里显示的是中文 displayName,回调给的是类型串如 "AND")。
    using TypeCallback = std::function<void(const std::string&)>;

    explicit ComponentPalette(wxWindow* parent);

    /// 用元件库填充(阶段四;MainFrame 在构造后调用一次)。
    void FillFromLibrary(const editor::ComponentLibrary& lib);
    void SetTypeCallback(TypeCallback cb);

private:
    void OnSelectionChanged(wxTreeEvent& evt);

    wxTreeCtrl* m_tree = nullptr;
    TypeCallback m_onType;
};
