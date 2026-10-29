# tools · GUI 验证工具(负责人 B)

本目录是开发期端到端验证工具，不编入主程序。开启 `BUILD_VERIFY_TOOLS=ON` 时，
现有 CMake 会单独构建 `ui_verify`；默认关闭，不需要修改共享构建文件。

## 为什么这样验证(GUI 自动化)

本机是 **Wayland 会话,XTest 注入鼠标无效**(指针 warp 不生效、点击投递不到 Xwayland
客户端,自动化会"静默不通过")。所以做法是:

1. `ui_verify.cpp` 起真实的 `UiMainFrame`,把 `wxMouseEvent` / `wxKeyEvent`
   **直接注入 `CanvasPanel` 的事件链**(窗口与渲染都是真的,只绕过 X 输入层);
2. Python 脚本驱动它,并用 `XGetImage` 抓窗口像素 + 读状态栏文本做断言。

## 编译验证台

```bash
cd <repo>
g++ -std=c++17 -I src $(wx-config --cxxflags) tools/ui_verify.cpp \
    src/ui/*.cpp src/model/*.cpp src/components/*.cpp src/io/*.cpp \
    src/simulation/*.cpp $(wx-config --libs core,base,propgrid) -o /tmp/ui_verify
```

### Windows

不依赖 `wx-config`,走 CMake(这个 target 默认不构建,不影响 `CircuitEditor`):

```bash
cmake -S . -B build -DBUILD_VERIFY_TOOLS=ON
cmake --build build --target ui_verify
python3 tools/verify_mvp.py build/ui_verify.exe    # MVP 链路 9 项(Windows 上若无 python3 就用 python)
```

Windows 上不用抓屏:`ui_verify` 加了一条 `pixel <x> <y>` 命令,用 `wxDC::GetPixel`
直接读画布颜色(替代 Linux 那边的 `XGetImage`),所以 `verify_mvp.py` 是跨平台的。
另外,`verify_mvp.py` 走临时文件重定向而不是 `capture_output` —— wx 的 GUI 程序
在 Windows 的 `CreateProcess` 管道下拿不到 stdout。

> **注意**:用 VS Code 的 CMake Tools 构建时,它 reconfigure 会读
> `.vscode/settings.json` 里的 `cmake.configureSettings`,那里没有 `BUILD_VERIFY_TOOLS`,
> target 就会消失。把 `"BUILD_VERIFY_TOOLS": "ON"` 一并加进那份配置即可(该文件已 gitignore)。

### Linux

`verify_mvp.py` 在 Linux 上也能跑,但有三处和 Windows 不一样:

- **程序名**没有 `.exe`(脚本会自动找 `build/ui_verify`,也可以显式传参)
- **画布尺寸**可能不同 —— 脚本第 0 项会先检查 ≥540×400,不够会直接报出实际尺寸
- **`wxDC::GetPixel` 在 wxGTK 上可能没实现** —— 这时脚本会打印 `n/a` 并**跳过视觉断言**,
  状态栏那几项照跑。像素核对接力仍由 `verify_stage4.py` 的 `XGetImage` 负责

```bash
cmake -S . -B build-linux -DBUILD_VERIFY_TOOLS=ON
cmake --build build-linux -j
python3 tools/verify_mvp.py build-linux/ui_verify
```

第一次跑请留意 `== 0. 画布尺寸 ==` 那一行。

## 跑核对脚本

```bash
ls /run/user/1000/.mutter-Xwaylandauth.*        # 取当前 XAUTHORITY(会话重启会变)
export DISPLAY=:0 XAUTHORITY=<上面那个> GDK_BACKEND=x11
python3 tools/verify_stage4.py      # 阶段四端到端:放置 → 连线 → 拨开关 → 拖动 → 删除(15 项)
python3 tools/verify_issues14.py    # 画布平移(issue 1)+ 局部重绘/性能(issue 4)(11 项)
```

两个脚本都会自己编译验证台到 `/tmp/ui_verify`,并把截图写到仓库上一级目录。

## 抓像素的坑(踩过,别再踩)

- **抓具体窗口**:`XGetImage(root…)` 在这台机器直接 `BadMatch`;用 `xwininfo -root -tree`
  找到标题含 `Circuit Editor` 的窗口 id 再抓。
- **通道顺序**:24 位深 32bpp 的 `XImage` 内存是 **B,G,R,X**;PIL 用 `frombytes("RGBX")`
  之后要**交换 R/B**,否则蓝色会被读成橙色。
- **别信 wx 报的屏幕坐标**:`ClientToScreen` / `GetScreenPosition` 与截图实际像素在 y 方向
  对不上,要用"已知逻辑位置的元件"标定画布原点(见 `verify_issues14.py`)。
- **网格比较注意周期**:粗网格线间距 200px,基准图要取"上一次平移之后"的。
- **栅格吸附**:放置 / 拖动都吸附 20 逻辑像素,脚本坐标直接用 20 的倍数。

## B · 第七周更新（2026-10-29）

`perf <n> <motions>` 每轮新建图纸后放置准确数量，输出 `n` 和 `actual`；仅用于验证台，
会替换验证台当前内容。`verify_issues14.py` 核对实际数量与原40ms门槛，不放宽阈值。
三个独立编译脚本的 UI 源文件列表已包含 `subcircuit_library.cpp`。

新功能和Bug定向测试见 `tests/b_subcircuit_test.cpp`、`tests/b_bug_audit.cpp`、
`tests/b_perf_count_probe.py`；构建/结果详见 [B-TODOLIST](../docs/B-TODOLIST.md)。
