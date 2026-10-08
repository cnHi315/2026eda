# 2026eda · 交接说明(负责人 B)

> 交接时间:2026-10-08(第 4 周末)
> 交接人:负责人 **B**(用户界面 / 绘图编辑,任务 2 + 任务 4)
> 读者:下一周接手 B 工作的人(或下一位 AI 助手)
> 一句话状态:**B 的四个阶段全部完成并已合入 `main`;画布已全面切到真实数据 + 仿真上屏,画布平移与性能优化(issue 1 / 4)也已完成。**
> 位置说明:本文件是**仓库内版本化副本**;工作区同级(`/home/shuai/EDA/HANDOFF.md`)另有一份内容一致的快捷入口。

---

## 0. 先看这三处(5 分钟进入状态)

| 看什么 | 为什么 |
| --- | --- |
| `2026eda/README.md` | 项目结构、分工、构建方式,以及 **"给 AI 助手的约定"六条**(必须先遵守) |
| `2026eda/docs/dev-log.md` 的 `第 4 周` | 本周计划 + 完成情况 + 遗留问题;B 的所有交付都记在这里 |
| `2026eda/docs/IMPLEMENTATION_ROADMAP.md` | **B 的 TODO List**,阶段一~五全部 `[x]`;下一周要加任务就往这里加 |

其余常用文档:`docs/interfaces.md`(接口契约 + 引脚表 + 几何参数表 + 交互手势表)、`docs/data-model.md`(数据结构与 net 规则)、`docs/test-plan.md`(手工核对表,含 T-01 ~ T-11)。

---

## 1. 代码与提交现状

- 仓库:`/home/shuai/EDA/2026eda`,分支 **`shuai`**,远程 `https://github.com/cnHi315/2026eda`
- **`origin/main` == `origin/shuai` == 本地 `shuai` == `fa4b980`**,工作区干净
- 最近提交(从新到旧):

| 提交 | 内容 | 作者 |
| --- | --- | --- |
| `fa4b980` | 画布体验与性能:视图平移(issue 1)+ 局部重绘与索引缓存(issue 4) | B |
| `a2178ac` | 阶段四:真数据上屏(模型 / 仿真 / 文件 / 属性表全接上) | B |
| `f39ecac` | 引脚模型定稿:SWITCH 单脚输出 / LED 单脚输入 + 几何参数与交互约定写入契约 | B |
| `7b415e2` | NetlistIO:保存 / 打开 JSON + 导出 KiCad 网表 | A |
| `cacdc49` | dev-log 格式规整 + 第 4 周计划 | A |

**角色分工**(改动各自模块,共享文件先问人):

| 角色 | 模块 | 对应任务 | 关键文件 |
| --- | --- | --- | --- |
| A | 数据模型 + 文件功能 | 任务 5 | `src/model/`、`src/io/`(含 `json.hpp`、`NetlistIO`) |
| **B** | 用户界面 + 绘图编辑 | **任务 2、4** | `src/main.cpp`、`src/ui/` |
| C | 元件库 | 任务 3 | `src/components/` |
| D | 电路仿真 | 任务 1、6 | `src/simulation/` |

> README 里的硬规矩:**不要改 `CMakeLists.txt` 和 `src/contract/data_model.h`**(要改先问 A);测试代码放 `tests/`;提交信息不要加 AI 署名;提交前必须本地编译过。B 本阶段没碰过这两个文件。

---

## 2. 环境与构建(本机已验证)

| 项 | 值 |
| --- | --- |
| 编译 | cmake 4.2.3、g++ 15.2.0 |
| GUI | wxWidgets **3.2.9**(GTK3,组件 `core base propgrid`) |
| 显示 | 物理 3072×1728,**缩放因子 2.0**;Wayland 会话 + Xwayland(`DISPLAY=:0`) |

```bash
cd /home/shuai/EDA/2026eda
cmake -S . -B build && cmake --build build -j      # 全量构建,应 0 warning / 0 error
./build/CircuitEditor                               # 启动(需要图形环境)
./build/model_test   # 57/57
./build/io_test      # 61/61
./build/sim_test     # 与门真值表 4/4 PASS
```

跑 GUI 的命令行姿势(XAUTHORITY 文件名每个会话都会变,先 `ls /run/user/1000/.mutter-Xwaylandauth.*` 找):

```bash
export DISPLAY=:0
export XAUTHORITY=/run/user/1000/.mutter-Xwaylandauth.XXXXXX
export GDK_BACKEND=x11                              # 不加的话 X 工具看不到窗口
./build/CircuitEditor
```

---

## 3. 推送代码(HTTPS 不可用,走 SSH-over-443)

本机 HTTPS 推送没有凭据(会报 `could not read Username`),**用 SSH-over-443**:

```bash
# 1) 若 /tmp/gh_known_hosts 不存在(重启后会丢),先重建并核对指纹
ssh-keyscan -p 443 ssh.github.com > /tmp/gh_known_hosts
ssh-keygen -lf /tmp/gh_known_hosts
#   必须与 GitHub 官方公布的三条一致:
#   RSA     SHA256:uNiVztksCsDhcc0u9e8BujQXVUpKZIDTMczCvj3tD2s
#   ECDSA   SHA256:p2QAMXNIC1TJYWeIOttrVc98/R1BUFWu3/LiyKgUfQM
#   ED25519 SHA256:+DiY3wvvV6TuJJhbpZisF/zLDA0zPMSvHdkr4UvCOqU

# 2) 推送(身份应为 ShuaiPr)
cd /home/shuai/EDA/2026eda
export GIT_SSH_COMMAND="ssh -o UserKnownHostsFile=/tmp/gh_known_hosts -o StrictHostKeyChecking=yes"
git push ssh://git@ssh.github.com:443/cnHi315/2026eda shuai:shuai
git push ssh://git@ssh.github.com:443/cnHi315/2026eda shuai:main   # 快进合并到 main
```

> 组里习惯:干完活直接把分支快进合到 `main`(B 这几周都这么做)。若 main 被别人推进过,先 `git fetch` 再 `git merge origin/main`,确认构建/测试通过后再推。

---

## 4. 当前产品能力(B 负责的部分)

| 能力 | 状态 | 说明 |
| --- | --- | --- |
| 三栏主窗口 | ✅ | 菜单栏(文件 / 编辑)+ 工具栏 + 状态栏(左:提示;右:元件 / 导线 / 网络计数)+ 元件库 / 画布 / 属性表 |
| 元件库 | ✅ | 由 `ComponentLibrary::types()/displayName()` 填充,**中文显示**(与门 / 或门 / 非门 / 开关 / LED);选中类型即进入放置模式 |
| 放置元件 | ✅ | 画布点空白 → `SchematicModel::addElement`,吸附 20 栅格,可连续放,Esc 退出;未知类型 → 状态栏提示且不创建 |
| 选中 / 拖动 | ✅ | 左键单击选中(单击不位移);拖动实时跟随、抬起吸附、Esc 还原;Delete 删除元件(连带断线)或导线 |
| 连线 | ✅ | 引脚按下 → 橡皮筋 → 目标引脚松开 → `model.addWire`;非法连线(自环 / 输出直连 / 重复)在状态栏给**原因** |
| 仿真上屏 | ✅ | 双击 SWITCH 拨动(左键单击已被选中/拖动占用);`Simulator::load + setInput + step`;导线按电平着色、LED 亮灭、开关底色变化 |
| 属性表 | ✅ | 显示选中元件(id / 类型 / 名称 / 位置 / 旋转 / 引脚表),**只读** |
| 文件功能 | ✅ | 文件菜单 + 工具栏:新建 / 打开 / 保存 / 另存为 / 导出网表(接 A 的 `NetlistIO`) |
| 画布平移 | ✅ | 中键拖动 / 滚轮 / Shift+滚轮(issue 1);网格锚定逻辑原点;内容至少留 40px 可视 |
| 局部重绘 | ✅ | 拖拽 / 橡皮筋 / 悬停只 `RefreshRect()`;网格按 `GetClippingBox()` 限定;`Find()` 有 id→指针索引缓存(issue 4) |
| 缩放 | ❌ | `m_scale` 预留在 `CanvasPanel`,目前恒为 1.0 |

### 关键契约(改之前务必看 `docs/interfaces.md`)

- **引脚模型**:`AND/OR` = A/B/Y(±50),`NOT` = A/Y,`SWITCH` = **单脚 Y(输出)**,`LED` = **单脚 A(输入)** —— 开关当源、LED 当汇。
  ⚠️ `NetlistIO::load()` 会**保留文件里的 pins 快照**,所以旧文件里可能还是两脚开关;UI 因此**不硬编码 pinIndex**,拨开关时对该元件所有引脚置同一电平。
- **几何参数(B 定稿)**:包围盒 60×40、引脚可视长度 20、引脚命中半径 8、导线容差 4、吸附步长 20、线宽 网格1/外框2/导线2/选中4、选中框外扩 5、引脚点 r2/r5、字号 8pt。
  唯一落地位置:`CanvasPanel::SizeOf(type)` 与 `kPinHitR / kWireHitR / kGridStep / kSymW / kSymH / kPinLen`。
- **交互手势**:左键选中 / 拖动、引脚拖出连线、**双击拨开关**、Delete 删除、Esc 取消、**中键或滚轮平移**。
- **电平着色**:高 = 亮绿 3px,低 = 暗绿 2px,未知(悬空)= 灰 2px;LED 高电平 = 橙色实心圆;开关闭合 = 体色变浅绿并显示 `1`。
- **开关状态**:默认**关闭**,只存在 UI 内存(`CanvasPanel::m_switchLevel`),v1 **不写进 JSON** —— 打开文件后所有开关回到关闭。

### 数据流

```
元件树(类型) ─┐
画布鼠标事件 ─┴→ CanvasPanel ──写──→ SchematicModel(addElement/moveElement/addWire/removeWire/removeElement/loadFrom)
                      │                        │
                      │读 data()                └─→ NetlistIO(save/load/exportNetlist)←─ 文件菜单
                      └──→ Simulator(load/setInput/step/query)→ 导线颜色 / LED / 开关底色
UiMainFrame 持有 model / lib / io / sim,负责菜单、状态栏计数、属性表联动
```

---

## 5. 怎么验证(GUI 自动化;这台机器的坑很多,务必看这段)

**结论先行:本机是 Wayland 会话,XTest 注入鼠标无效**(指针 warp 不生效、点击投递不到 Xwayland 客户端,会"静默不生效")。
可行做法:**在进程内直接向 wx 事件系统注入 `wxMouseEvent` / `wxKeyEvent`,窗口与渲染都是真的;再用 `XGetImage` 抓窗口像素做断言。**

### 5.1 验证台(`tools/ui_verify.cpp`,已入库)

编译命令见 `tools/README.md`(不需要改 `CMakeLists.txt`)。结构要点:

1. 用 `wxIMPLEMENT_APP`,在 `OnInit` 里 `new UiMainFrame()` 并 `Show()`;
2. 递归 `dynamic_cast<CanvasPanel*>` 找到画布子窗口;
3. 用 120ms 的 `wxTimer` 从 stdin 逐行读命令(每执行完打印 `OK <原命令>`)。命令集:`place <TYPE> <x> <y>` / `down|move|up <x> <y>` / `dclick` / `mdown|mup` / `wheel <x> <y> <rot> [h]` / `esc|del` / `info` / `perf <n> <motions>` / `quit`;
4. 事件这样造并派发(全流程关键):

   ```cpp
   wxMouseEvent evt(wxEVT_LEFT_DOWN);   // 或 LEFT_UP / MOTION / LEFT_DCLICK / MIDDLE_DOWN …
   evt.SetEventObject(canvas);
   evt.SetPosition(wxPoint(x, y));      // 画布客户区坐标 = 逻辑坐标
   if (type == wxEVT_LEFT_DOWN) evt.SetLeftDown(true);
   canvas->ProcessWindowEvent(evt);     // 同步走真实事件链
   canvas->Refresh(); canvas->Update(); // 立刻重绘,便于外部抓图
   ```

5. `info` 命令打印 `GetTitle()` / `GetStatusBar()->GetStatusText(0|1)` / `ClientToScreen(0,0)` / `GetClientSize()`,供外部脚本核对;
6. 编译(不需要改仓库的 CMake):

   ```bash
   g++ -std=c++17 -I 2026eda/src $(wx-config --cxxflags) ui_verify.cpp \
       2026eda/src/ui/*.cpp 2026eda/src/model/*.cpp 2026eda/src/components/*.cpp \
       2026eda/src/io/*.cpp 2026eda/src/simulation/*.cpp \
       $(wx-config --libs core,base,propgrid) -o /tmp/ui_verify
   ```

### 5.2 抓像素的坑(踩过,别再踩)

- **必须抓具体窗口**:`XGetImage(root…)` 在这台机器直接 `BadMatch`;用 `xwininfo -root -tree` 找到标题含 `Circuit Editor` 的窗口 id,对它调用 `XGetImage`。
- **通道顺序**:24 位深 32bpp 的 `XImage` 内存里是 **B,G,R,X**。PIL 里 `frombytes("RGBX")` 之后要**交换 R/B**,否则蓝会被读成橙(选中的蓝色外框会"消失")。
- **坐标别信 wx 的屏幕坐标**:`ClientToScreen` / `GetScreenPosition` 与截图里的实际像素在 y 方向对不上。**用已知逻辑位置的元件做标定**(例如测得 U1 矩形中心像素 − 2×(280,240) = 画布原点),之后所有测量都基于这个标定值。
- **比较网格位移注意周期**:粗网格线间距 200px(物理),基准图要取"上一次平移之后"的,否则会 `≡ 20 (mod 200)` 绕圈。
- **栅格吸附**:放置和拖动都吸附 20 逻辑像素,脚本坐标直接用 20 的倍数(例如 `y=230` 会被吸到 `240`)。
- **抓取偏移**:按下的位置若不是元件中心,`grab` 偏移会带到抬起落点(按下点比中心高 10px,落点就多 10px)。

### 5.3 两套脚本与结果(源码在 `tools/` 下,可直接复跑)

| 脚本 | 内容 | 结果 |
| --- | --- | --- |
| `tools/verify_stage4.py` | 阶段四端到端:放置 4 元件 → 连 3 条线 → 选中高亮 → 双击拨两个开关 → 混合输入 → 拖动 → 删除 | **15/15 通过** |
| `tools/verify_issues14.py` | issue 1/4:平移位移量、平移后命中与拖动、无残影、网格随平移、滚轮一格、性能 | **11/11 通过**;400 元件 13.5ms/帧、1000 元件 32.8ms/帧 |

---

## 6. 未完成 / 待外部输入(下周优先看)

### 需要别人点头(不是 B 能单方面做完的)

1. **C:真符号外观(v1.1)** —— 现在五个元件一律"矩形 + 引脚名"。要做弧线(与门 / 或门)、非门输出圆圈、开关拨杆、LED 三角箭头,需要 C 给出**几何描述**(折线 / 圆 / 弧的坐标),否则画不准。
2. **C:追认两件已定稿的事** —— (a) `docs/interfaces.md` 里 B 定的几何参数表;(b) SWITCH/LED **单脚**模型(由 B 代改 `src/components/component_library.cpp`,连带改了 `tests/model_test.cpp`、`tests/io_test.cpp`)。
3. **C:未知类型语义** —— B 已自定行为(不创建 + 状态栏提示 + 打开文件不丢数据),需要 C 确认"`pinTemplate()` 返回空 = 类型不可用"。
4. **D:`Simulator::step()` 目前固定迭代 2 次** —— 两级链路(开关→与门→LED)够用;演示时若多插一级门,电平传不到底、LED 会停在未定义。建议改成"迭代到稳定 + 上限"。另:`setSignalCallback` 的触发时机与单次 `step()` 回调上限仍未定(B 现在不依赖回调,直接 `query()` 着色)。
5. **A:若要把 GUI 自动化验证台收进仓库** —— 会动 `CMakeLists.txt`(加测试目标),按 README 约定必须先问 A。

### B 自己可以接着做的候选(建议优先级从上到下)

| 候选 | 说明 | 依赖 |
| --- | --- | --- |
| T-04 / T-05 人工核对 | 保存 / 打开 / 导出网表的**文件对话框**没被自动化覆盖(`io_test` 覆盖了 IO 本身),需人工点一次走通 | 无 |
| `docs/user-manual.md` 更新 | 现在是阶段一内容,要补:放置 / 连线 / 拨开关 / 平移 / 保存导出 的操作与快捷键 | 无 |
| 答辩演示脚本 | 一条"从零到 LED 亮"的截图 / 录屏流程,配 `docs/test-plan.md` 的 T-02~T-07 | 无 |
| 撤销 / 重做(F-06,Could) | 现在 Delete 删掉没法恢复;需要给 `SchematicModel` 加快照或命令栈(**会碰 A 的模块,先商量**) | A |
| 画布缩放 | `m_scale` 已预留,加 Ctrl+滚轮缩放 + 命中 / 线宽换算;注意 HiDPI 的 `FromDIP` 坑 | 无 |
| 网格位图缓存 | issue 4 的第 1 条建议;当前 1000 元件 32.8ms/帧够用,规模更大再上 | 无 |
| v1.1 真符号 | 见上面第 1 条 | C |

---

## 7. 下周开工清单(照着做即可)

```bash
# 1) 拉最新 + 建环境
cd /home/shuai/EDA/2026eda
export GIT_SSH_COMMAND="ssh -o UserKnownHostsFile=/tmp/gh_known_hosts -o StrictHostKeyChecking=yes"
git fetch ssh://git@ssh.github.com:443/cnHi315/2026eda 'refs/heads/*:refs/remotes/origin/*'
git merge --ff-only origin/main
cmake -S . -B build && cmake --build build -j && ./build/model_test && ./build/io_test && ./build/sim_test

# 2) 看本周(第 5 周)计划;若还没写就自己补到 dev-log
sed -n '/## 第 5 周/,$p' docs/dev-log.md

# 3) 手动跑一遍 GUI 确认没坏
ls /run/user/1000/.mutter-Xwaylandauth.*      # 取当前的 XAUTHORITY
export DISPLAY=:0 XAUTHORITY=<上面那个> GDK_BACKEND=x11
./build/CircuitEditor                          # 放两个开关 + 与门 + LED → 连线 → 双击开关看 LED

# 4) 想跑自动化(脚本会自己编译 /tmp/ui_verify)
python3 tools/verify_stage4.py      # 阶段四端到端,期望 15/15
python3 tools/verify_issues14.py    # 画布平移 + 性能,期望 11/11
```

---

## 8. 坑位清单(踩过一次就够了)

1. **非 ASCII 文本必须走 `wxString::FromUTF8`**(代码里统一用 `U8("…")`)。窄字面量在这台机器的 locale 下会被**吞成空串** —— 窗口标题曾因此只剩文件名,`"%d°"` 也一样。
2. **wxDC 与鼠标事件在 2.0 缩放下已经是 DIP 单位**:逻辑坐标 / 命中半径 / 线宽**不要再套 `FromDIP()`**(会二次放大);`FromDIP` 只用于窗口尺寸、位图这类物理像素场合。
3. **XTest 在本机无效**,自动化一律走 §5.1 的进程内事件注入。
4. **抓图要抓窗口、要交换 R/B 通道**。
5. **坐标标定别信 wx 的屏幕坐标**(见 §5.2)。
6. **引脚模型是全组隐式契约**:改一行 `pinTemplate` 会连带 A 的 `model_test` / `io_test`(当时共 9 处断言),改之前先说。
7. `pkill -f './build/CircuitEditor'` 会把执行该命令的 shell 自己也杀掉 —— 用 `pkill -x CircuitEditor`。
8. **网格吸附 20**:脚本 / 示例坐标直接用 20 的倍数,免得位置对不上。
9. 工作区目录(`/home/shuai/EDA`)里原先的 `B_TODOLIST.md`、issues 底稿与截图已被清理;验证脚本已随本次提交收进 `tools/`,issue 状态在本文件 §9 重述。

---

## 9. 历史 issue 状态(底稿文件已不在,这里保留结论)

| # | 标题 | 状态 |
| --- | --- | --- |
| 1 | 画布支持滚动 / 平移 | ✅ 已完成(中键拖动 + 滚轮;网格锚定原点;`ClampOrigin`) |
| 2 | HiDPI 下网格密度 / 线宽未自适应 | ✅ 误报:wx 已是 DIP 单位,逻辑坐标下**不能**再 `FromDIP()` |
| 3 | 元件符号尺寸与 relPos 未与 C 对齐 | 🟡 relPos(±50)已对齐;**几何参数由 B 定稿并写进 `interfaces.md`**,待 C 追认;真符号外观留 v1.1 |
| 4 | 元件多时全量重绘的性能问题 | ✅ 已完成(`RefreshRect` + 网格裁切 + 索引缓存;1000 元件 32.8ms/帧) |
| 5 | SWITCH / LED 引脚模型不一致(两脚 vs 单脚) | 🟡 已采用**单脚**并落地(含 A 的两个测试),待 C / D 追认 |
| 6 | 未知元件类型时 UI 行为未定义 | 🟡 B 已自定(不创建 + 状态栏提示 + 打开文件不丢数据),待 C 确认语义 |

---

## 10. 一分钟自查(开工前确认环境没坏)

- [ ] `git status` 干净,`git log -1` 是 `fa4b980`(或更新的 main)
- [ ] `cmake --build build -j` → 0 warning / 0 error
- [ ] `model_test 57/57`、`io_test 61/61`、`sim_test PASS`
- [ ] 启动 `CircuitEditor`:左侧元件树 5 项是**中文**;放两个开关 + 与门 + LED,连线,双击开关 → 导线变亮绿、LED 变橙色
- [ ] 中键拖动 / 滚轮能平移画布,松开后状态栏报原点偏移
