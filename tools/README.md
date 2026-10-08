# tools · GUI 验证工具(负责人 B)

这目录**不属于构建**:`CMakeLists.txt` 的 `GLOB_RECURSE` 只收 `src/`,这里不会被编进
`CircuitEditor`,也不会给 `cmake --build` 增加任何目标。它们是开发期用来做端到端核对的脚本。

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
