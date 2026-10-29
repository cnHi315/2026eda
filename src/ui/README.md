# UI 模块（B：任务 2、4）

| 文件 | 职责 |
| --- | --- |
| main_frame | 三栏窗口、菜单/工具栏/状态栏、文件功能和子电路封存/刷新 |
| canvas_panel | 绘制、框选/整组编辑、连线、子电路放置、仿真上屏、撤销/重做 |
| component_palette | 五类内置元件与自定义子电路树 |
| property_panel | 单选只读属性、多选数量 |
| subcircuit_library | 普通 Schematic JSON 片段的保存、扫描及兼容性检查 |

第七周进度、Bug状态、操作和测试见 [B-TODOLIST](../../docs/B-TODOLIST.md)。几何/交互约定见 [interfaces](../../docs/interfaces.md)。

model/io/components/simulation 接口由对应成员维护，UI 通过共享契约调用；不改变文件格式或后端签名。定向测试在 tests/，现有 CMake 自动收集新增 UI 源文件。
