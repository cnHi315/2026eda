#pragma once
// 主窗口(负责人 B):菜单栏 + 工具栏 + 状态栏 + 三栏布局。
// 布局:左 = 元件库(ComponentPalette),中 = 画布(CanvasPanel),右 = 属性表(PropertyPanel)。
// 阶段一为静态骨架:不含交互,也不依赖 model/components/io/simulation。

#include <wx/frame.h>

class ComponentPalette;
class CanvasPanel;
class PropertyPanel;

class UiMainFrame : public wxFrame {
public:
    UiMainFrame();

private:
    void BuildMenuBar();
    void BuildToolBar();
    void BuildStatusBar();
    void BuildLayout();

    ComponentPalette* m_palette = nullptr;
    CanvasPanel* m_canvas = nullptr;
    PropertyPanel* m_props = nullptr;
};
