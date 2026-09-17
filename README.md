# 2026eda · 简易电路原理图编辑器

类 Logisim 的数字逻辑电路设计与仿真工具(C++17 + wxWidgets)。

> 当前状态:**阶段 0 收尾**。整条编译链已验证(能弹出主窗口)。
> 下一步:其余三人配好环境,然后按分工各自开工。

---

## 一、技术栈

| 项 | 选型 |
| --- | --- |
| 语言 | C++17 |
| GUI | wxWidgets 3.2.2 |
| 构建 | CMake ≥ 3.16 + wxWidgets 3.2.x,**只有一个 CMakeLists.txt**(Windows/Linux 同一份) |
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

- **除 ui 外,任何模块禁止出现 `#include <wx/...>`**(方便各自单独编译)。
- 依赖方向:ui 调用其余四个模块;**components 是叶子**,model 和 simulation 依赖它。
- 加新 .cpp 不用改 CMakeLists(自动收集)。
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

需要 CMake ≥ 3.16 + wxWidgets 3.2.x。同一份 CMakeLists.txt 两端通用,只是 wxWidgets 的找法不同。

### Windows

```bash
# wxWidgets 路径换成自己机器上的;下面这组是本机已验证的
cmake -S . -B build -G "MinGW Makefiles" \
      -DwxWidgets_ROOT_DIR=G:/computer/wxWidgets-3.2.2.1 \
      -DwxWidgets_LIB_DIR=G:/computer/wxWidgets-3.2.2.1/lib/gcc_lib
cmake --build build -j
```

> VS Code:把上面两项填进 `.vscode/settings.json` 的 `cmake.configureSettings`,之后用 CMake Tools 构建(该文件已 gitignore,各人配各人的)。

### Linux

```bash
sudo apt install -y build-essential cmake git libwxgtk3.2-dev
./build.sh        # 等价于 cmake -S . -B build && cmake --build build -j
```

### 验收

跑起来弹出标题 `Circuit Editor` 的窗口(菜单栏 + 工具栏 + 状态栏)—— 说明编译、链接、运行三链全通。
**看到窗口之前不要开始写自己那块**,先在群里同步环境问题。

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
