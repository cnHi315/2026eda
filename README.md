# Circuit Editor

> 类 Logisim 的工业级数字逻辑电路设计与仿真工具（C++17 + wxWidgets）

本仓库是团队协作开发的**数字电路编辑器**。当前处于 **契约驱动开发（Contract-First）** 阶段：接口契约已冻结，UI 层与 Core 层可并行开展各自的 TDD 切片。

---

## 一、项目简介

### 技术栈

| 项 | 选型 |
| --- | --- |
| 语言标准 | C++17 |
| GUI 框架 | wxWidgets 3.2 |
| 构建系统 | CMake ≥ 3.16 |
| 单元测试 | GoogleTest v1.15.2（CMake FetchContent 自动拉取，无需手动安装） |

### 架构：单向依赖，严格解耦

```
┌──────────────────────────┐
│   ui   (wxWidgets 表现层) │   ← 仅此层允许出现 wx* 类型
└────────────┬─────────────┘
             │ 依赖（单向）
             ▼
┌──────────────────────────┐
│   core (纯逻辑，无 GUI)   │   ← 禁止 include 任何 wx 头文件
└──────────────────────────┘
```

- `ui` → `core` **单向依赖**；`core` 层必须能在**无图形环境**下独立编译与测试。
- 两层之间**唯一**的耦合边界是契约头文件：`include/core_interfaces.h`
  （`ICircuitElement` / `INetlistModel` / `ISimulationObserver`）。
- 坐标等基础类型使用 POD（如 `core::Point`），**禁止**把 `wxPoint` / `wxSize` 泄漏进 core。

### 模块划分与分工

| 模块 | 负责人 | 说明 |
| --- | --- | --- |
| 模块 1 用户界面 / 模块 3 绘图与编辑 | UI（本仓库前端） | 见 `docs/IMPLEMENTATION_ROADMAP.md` |
| 模块 2 元件库（数据层） | Role A | 见 `docs/BACKEND_TODO.md` |
| 模块 5 仿真引擎 | Role B | 见 `docs/BACKEND_TODO.md` |
| 模块 4 文件功能 / 网表导入导出 | Role C | 见 `docs/BACKEND_TODO.md` |

> 动手前请先阅读 `docs/BACKEND_TODO.md`（Core 组员）或 `docs/IMPLEMENTATION_ROADMAP.md`（UI 组员），认领属于自己的 TDD 切片。

---

## 二、Ubuntu 环境依赖

在 Ubuntu 上安装编译工具链、构建系统与 wxWidgets 开发包：

```bash
sudo apt update
sudo apt install -y build-essential cmake git libwxgtk3.2-dev
```

> - GoogleTest **不需要** apt 安装：首次配置时由 CMake 通过 `FetchContent` 从 GitHub 拉取，
>   因此**首次配置需要联网**（会 `git clone` googletest，可能需要几分钟）。
> - 若你的发行版 wxWidgets 版本较旧，请将 `libwxgtk3.2-dev` 换成对应版本的开发包
>   （例如 `libwxgtk3.0-gtk3-dev`）。

### 环境自检

```bash
wx-config --version     # 期望输出 3.2.x
cmake --version         # 期望 >= 3.16
g++ --version
```

---

## 三、标准 MVT 启动步骤（cmake & make）

在**仓库根目录**（即本文件所在目录）执行：

```bash
# 1. 配置（首次会拉取 GoogleTest，需要联网）
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# 2. 编译
cmake --build build -j"$(nproc)"

# 3. （可选）运行 Core 层单元测试
ctest --test-dir build --output-on-failure

# 4. 启动 GUI
./build/ui/CircuitEditor
```

也可以直接使用一键脚本完成「配置 + 编译 + 测试」：

```bash
./build.sh
```

> **注意可执行文件路径**：可执行文件输出在 `build/ui/CircuitEditor`，
> 而**不是** `build/CircuitEditor`。直接 `./build/CircuitEditor` 会报 `No such file`。

### 无图形环境 / 远程服务器

GUI 需要显示服务（X11 或 Wayland）。若在无桌面的服务器上，可用虚拟显示：

```bash
sudo apt install -y xvfb
xvfb-run -a ./build/ui/CircuitEditor
```

---

## 四、验收标准（务必遵守）

> **必须在本机成功弹出一个空白主窗口，才算环境就绪。**

窗口标题为 `Circuit Editor`，包含菜单栏、工具栏与状态栏即可 —— 这表示**编译链、链接链、运行链**三条链路全部打通。

**在看到该窗口之前，请不要开始各自的 TDD 切片开发。** 如果卡在环境问题上，先与团队同步，避免各自踩坑。

---

## 五、目录结构

```
circuit_editor/
├── CMakeLists.txt              # 顶层构建配置（wxWidgets / GoogleTest / 子目录）
├── build.sh                    # 一键构建 + 测试脚本
├── include/
│   └── core_interfaces.h       # ★ ui ↔ core 唯一契约（已冻结，改动需团队评审）
├── core/                       # 纯逻辑静态库（无 wx 依赖）
│   ├── CMakeLists.txt
│   ├── include/editor/core/
│   └── src/
├── ui/                         # wxWidgets 表现层可执行目标
│   ├── CMakeLists.txt
│   └── src/
├── tests/                      # GoogleTest 单元测试（仅依赖 core）
│   ├── CMakeLists.txt
│   └── test_core.cpp
└── docs/
    ├── IMPLEMENTATION_ROADMAP.md   # UI 组员四阶段开发路线图
    └── BACKEND_TODO.md             # Core 组员（Role A/B/C）任务拆解
```

---

## 六、协作约定

- **契约优先**：`include/core_interfaces.h` 为已冻结契约。任何修改都需同步通知全组，并同步更新 `docs/BACKEND_TODO.md`。
- **TDD 节奏**：先写失败测试（红）→ 最小实现（绿）→ 重构。提交前确保 `ctest` 全绿。
- **不要提交构建产物**：`build/` 等目录已在 `.gitignore` 中忽略，请勿强制加入。
- **分层红线**：core 层代码中**不得**出现 `#include <wx/...>`；提交前可用如下命令自查：

```bash
grep -rn "#include <wx/" core/ && echo "❌ core 层混入 wx 依赖" || echo "✅ core 层保持纯净"
```
