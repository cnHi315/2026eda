# 2026eda · 简易电路原理图编辑器

类 Logisim 的数字逻辑电路设计与仿真工具(C++17 + wxWidgets)。

> 当前状态:**阶段 0 —— 工具熟悉**。工程骨架已就绪(已验证:空白主窗口能弹出)。
> 下一步:全组装好 VS2022 + wxWidgets 3.2.2,跑通最小示例工程。

---

## 一、技术栈

| 项 | 选型 |
| --- | --- |
| 语言 | C++17 |
| GUI | wxWidgets 3.2.2 |
| 构建 | CMake ≥ 3.16(Windows:VS2022;Linux:make),**只有一个 CMakeLists.txt** |
| 文件格式 | 保存/打开用 JSON(开工时引入 nlohmann/json 单头文件);网表为简化文本格式 |
| 测试 | 手工核对表 [docs/test-plan.md](docs/test-plan.md)(不引入单元测试框架) |

## 二、结构约定:一个 src,五个模块

```menu
src/
├── main.cpp        程序入口(B 维护)
├── contract/       ★ 唯一契约 data_model.h —— 改它必须全组同意并更新 docs
├── ui/             界面(B):窗口、画布、元件面板、属性表
├── model/          数据模型(A):SchematicModel
├── components/     元件库(C):ComponentLibrary、各门元件
├── io/             文件功能(A):NetlistIO(保存/打开/导出网表)
└── simulation/     仿真(D):Simulator
```

规则:

- **model/components/io/simulation 里禁止出现 `#include <wx/...>`**(与界面无关,方便单独测试)。
- 加新 .cpp 不用改 CMakeLists(自动收集)。
- 依赖方向:ui 调用下面四个模块;四个模块互相不依赖,只依赖 contract。
- 详见 [docs/architecture.md](docs/architecture.md)。

## 三、分工

| 模块 | 负责人 | 任务 |
| --- | --- | --- |
| 架构 / 文件(model + io) | A | 任务 5、集成 |
| GUI / 绘图编辑(main + ui) | B | 任务 2、任务 4 |
| 元件库(components) | C | 任务 3 |
| 仿真(simulation) | D | 任务 1、任务 6 |

任务拆解见 [docs/BACKEND_TODO.md](docs/BACKEND_TODO.md)(A/C/D)与 [docs/IMPLEMENTATION_ROADMAP.md](docs/IMPLEMENTATION_ROADMAP.md)(B)。

## 四、构建

### Windows(主力环境,VS2022)

1. 安装 VS2022 与 wxWidgets 3.2.2,并建好 `$(WXWIN)` 环境变量(指向解压的 wxWidgets 目录);
2. 命令行构建:

   ```bat
   cmake -S . -B build -DwxWidgets_ROOT_DIR=%WXWIN%
   cmake --build build --config Debug
   ```

   或在 VS2022 里直接"打开本地文件夹"(CMake 项目)。
3. 可执行文件在 `build/Debug/CircuitEditor.exe`。

> 找不到 wxWidgets 时,先检查 `$(WXWIN)` 环境变量;Debug/Release 与库版本要对应。

### Ubuntu

```bash
sudo apt install -y build-essential cmake git libwxgtk3.2-dev
cmake -S . -B build
cmake --build build -j"$(nproc)"
./build/CircuitEditor
```

或一键:`./build.sh`。

### 验收:能弹出空白主窗口

窗口标题 `Circuit Editor`,含菜单栏、工具栏、状态栏 —— 说明编译链、链接链、运行链全通。
**看到窗口之前不要开始各模块开发**,先和组内同步环境问题。

## 五、协作约定

- **契约优先**:改 `src/contract/data_model.h` 或四个类的签名,先在群里说一声,并同步更新 [docs/data-model.md](docs/data-model.md)、[docs/interfaces.md](docs/interfaces.md)。
- 加新元件/新功能,顺手在 [docs/test-plan.md](docs/test-plan.md) 加一行核对项。
- 不要提交构建产物(build/ 已忽略);安装包等大文件(resources/)不入库。
- 每周在 [docs/dev-log.md](docs/dev-log.md) 记几行:完成、问题、下周计划。

## 六、文档

| 文件 | 用途 |
| --- | --- |
| [docs/requirement.md](docs/requirement.md) | 需求与验收标准(任务分解) |
| [docs/architecture.md](docs/architecture.md) | 分层与依赖规则 |
| [docs/data-model.md](docs/data-model.md) | 数据结构(最重要) |
| [docs/interfaces.md](docs/interfaces.md) | 四个类的接口约定 |
| [docs/test-plan.md](docs/test-plan.md) | 手工测试核对表 |
| [docs/user-manual.md](docs/user-manual.md) | 演示/答辩用的操作说明 |
| [docs/dev-log.md](docs/dev-log.md) | 开发日志 |
