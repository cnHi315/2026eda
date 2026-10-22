# UI 美化方案 · Codex 风格 + 材质分层

> 状态：☑ 评审中 ｜ 负责人：B ｜ 最后更新：2026-10-22
> 范围：**只动 `src/ui/`**；不碰 `src/contract/data-model.h`、不改几何参数（60×40 / 引脚 20 / 命中 8 / 吸附 20）、不改任何功能与交互语义。
> 关联：[user-manual.md](user-manual.md)、[demo-script.md](demo-script.md)、[test-plan.md](test-plan.md)

## 〇、结论速览

| 问题 | 结论 |
| --- | --- |
| Codex 风格能不能落地？ | **能**。它本质是一套 token（surface / ink / accent / contrast / opaqueWindows / variant），wxWidgets 完全做得到，工时 1.5～2 天 |
| 液态玻璃能不能做？ | **只能做到 T1「毛玻璃卡片」**：半透明分层 + 高光边 + 内阴影。不是苹果那种会扭曲背景的液态玻璃 |
| 光折射（refraction）？ | **做不了**。wxWidgets 3.2 没有 backdrop 采样、没有 shader；要做得换 `wxGLCanvas` 重写渲染管线，超出收尾期 |
| 最大风险？ | 改颜色会打挂 5 套自动化核对（见 §三）；`wxPropertyGrid` 在 GTK 下几乎无法主题化 |

---

## 一、现状盘点

| 维度 | 现在 | 问题 |
| --- | --- | --- |
| 布局 | 三栏 `wxBoxSizer`：左 200 / 中拉伸 / 右 260，四周各 4px | 面板间只有空白，**没有分隔与层次**；标题就是普通 `wxStaticText` |
| 颜色 | `canvas_panel.cpp` 里散落 **18 处 `wxColour(...)` 字面量** | 无 token，改一处要翻遍全文件 |
| 字体 | 引脚 8pt / 开关态 10pt 粗 / 元件名 11pt | 没有字号阶梯；8pt 在投影上偏小 |
| 工具栏 | `wxArtProvider` 系统图标 ×6，无分组 | 彩色/单色混杂，不统一 |
| 画布 | 网格 `(235,235,235)` + `(215,215,215)`；元件白底 2px 黑框 | **网格对比度偏高**，抢主体 |
| 状态栏 | 两格：左长文本 + 右计数 | 计数没有视觉分隔 |
| 主题 | 无，只有浅色 | 无 dark/light；HiDPI 部分尺寸硬编码 |

---

## 二、红线：这些断言不许打挂

美化一旦改色，这几处自动化核对会直接失败 —— **先看再动**：

| 位置 | 约束 |
| --- | --- |
| `tools/verify_mvp.py:131` | LED 熄灭时画布像素 `r>240 && g>240 && b>240` → **画布底色必须保持纯白** |
| `tools/verify_mvp.py:135` | 高电平导线 `g>150 && r<60 && b<60` → 绿色必须**保持低蓝分量**（`#22C55E` 的 b=94 会挂） |
| `tools/verify_mvp.py:137` | LED 点亮 `r>200 && 100<g<220 && b<60` |
| `tools/verify_stage4.py:228` | 选中态**蓝色**像素数 > 300 |
| `tools/verify_stage4.py:243-255` | **亮绿**导线像素数、**浅绿**开关底色像素数的相对变化 |
| `tools/verify_issues14.py` | 画布平移/局部重绘的像素比对（依赖网格与元件颜色稳定） |

> **结论**：美化只能在「色相不变、调明度/饱和度/线宽/层次」范围内做；画布色一律不动。要换主色，必须同步改脚本阈值。

---

## 三、Codex 的 token 模型

参考 [codex-themes](https://github.com/samuxbuilds/codex-themes/blob/main/docs/theme-snippets.md)，Codex 的主题就是一份结构化 token：

```json
{ "variant": "dark", "surface": "#0d1117", "ink": "#c9d1d9",
  "accent": "#58a6ff", "contrast": 58, "opaqueWindows": false,
  "fonts": { "ui": null, "code": null },
  "semanticColors": { "diffAdded": "#3fb950", "diffRemoved": "#f85149" } }
```

六个概念就够：**`surface` 底 / `ink` 字 / `accent` 强调 / `contrast` 对比度旋钮 / `opaqueWindows` 是否半透明 / `variant` dark|light**。

> `opaqueWindows` 说明 Codex 本身就支持半透明窗口 —— 所以「Codex 风格」与「玻璃材质」并不冲突，它本来就是一个可开关的材质参数。

---

## 四、两套 token

| token | Codex Dark | Codex Light | 用在哪 |
| --- | --- | --- | --- |
| `surface` | `#0D1117` | `#FFFFFF` | 主背景 |
| `surface-raised` | `#161B22` | `#F6F8FA` | 面板 / 工具栏 / 状态栏 |
| `border` | `#30363D` | `#D0D7DE` | 1px 分隔线 |
| `ink` | `#C9D1D9` | `#24292F` | 正文 |
| `ink-muted` | `#8B949E` | `#57606A` | 次要文字 / 引脚名 |
| `accent` | `#58A6FF` | `#0969DA` | 选中 / 焦点 / 可交互 |
| `semantic.ok` | `#3FB950` | `#1A7F37` | 高电平 / 成功 |
| `semantic.warn` | `#D29922` | `#9A6700` | 环路警告 / 非法连线 |
| `semantic.err` | `#F85149` | `#CF222E` | 打开失败 |
| `contrast` | 58 | 38 | 全局对比度旋钮 |

**画布底色两套都是 `#FFFFFF`** —— 这是 §二 的硬约束决定的，也符合 Figma / AutoCAD / Logisim 的「深色界面 + 白色工作纸」惯例。

### 尺寸阶梯（`theme.h` 里统一定义，全部走 `FromDIP`）

```cpp
constexpr int kFontXs = 8, kFontSm = 9, kFontMd = 10, kFontLg = 12;
constexpr int kSpace1 = 4, kSpace2 = 8, kSpace3 = 12, kSpace4 = 16;
constexpr int kStrokeThin = 1, kStrokeBase = 2, kStrokeThick = 3;
```

---

## 五、材质分层：液态玻璃 / 光折射的可行性

| 层级 | 做法 | 可行性 | 工时 | 判断 |
| --- | --- | --- | --- | --- |
| **T0 不透明** | 纯色 + 1px 边框（Codex 默认） | ✅ | 0 | 打底 |
| **T1 半透明玻璃** | `wxGraphicsContext` + RGBA 填充 + 顶部 1px 高光 + 底部内阴影 | ✅ **推荐** | 1–2 天 | 只用在**浮层**（工具栏 / 属性卡 / 弹窗） |
| **T2 真·背景模糊** | 抓背景 `wxBitmap` → 3 次 box blur ≈ 高斯 → 面板底图 | ⚠️ 可做但重 | 2–4 天 | 只给弹窗；**主画布上禁用**（已 33–39ms/帧） |
| **T3 真·光折射** | 背后像素 + UV 位移贴图 + 法线/SDF，即 GPU shader | ❌ **不可行** | — | wx 3.2 无 shader、无 backdrop 采样 |
| **T4 系统亚克力** | Windows `SetWindowCompositionAttribute`（非公开 API） | ⚠️ 仅 Windows | 1 天 | 跨平台演示不成立，不建议 |

**为什么 T3 不行**：折射需要「读取背后的像素 → 按位移贴图重采样 → 输出」。`wxDC` 是纯前向绘制，读不到被遮挡内容；`wxGraphicsContext`（Windows 上是 GDI+/Direct2D、GTK 上是 Cairo）也没有 backdrop filter 或 shader 入口。唯一现实路径是把画布换成 `wxGLCanvas` + 自写 shader —— 那是新项目量级。

---

## 六、布局改法（Codex 风格）

| 区域 | 现在 | 改成 |
| --- | --- | --- |
| **顶部工具栏** | 6 个彩色系统图标，无分组 | 单色线性图标 16/20px，`编辑(撤销/重做) │ 文件(新建/打开/保存/导出)`，高 32px，`surface-raised` 底 |
| **左·元件库** | `wxTreeCtrl` 白底 | 宽 200，`surface-raised`，分组标题 11px 大写 + 字间距，图标 + 中文名，hover 行高亮 |
| **中·画布** | 白底，网格偏重 | 白底保留；网格弱化 `#F6F7F9` / `#E8EAEE`；四周留 8px `surface` 垫层形成「纸浮在桌上」 |
| **右·属性表** | `wxPropertyGrid` 默认样式 | ⚠️ **最大风险点**：`wxPropertyGrid` 在 GTK 下几乎无法主题化。要 Codex 那种等宽 key-value，需换成自绘 `wxVListBox`（+1 天） |
| **底部状态栏** | 两格 | 三段：左=操作提示 / 中=模式 / 右=`元件 4 · 导线 3 · 网络 3`，**等宽字体**，高 22px |
| **字体** | 全默认 | `fonts.ui`=系统 sans（Segoe UI / Noto Sans CJK），`fonts.code`=等宽（坐标 / 引脚 / 网表 / 计数） |
| **加分项** | — | **命令面板 `Ctrl+K`** —— Codex 感最强的一处，约半天 |

---

## 七、分阶段落地

| 阶段 | 内容 | 工时 | 风险 |
| --- | --- | --- | --- |
| **P0** | `theme.h`（两套 token + 尺寸阶梯）+ 面板/工具栏/状态栏换色 + 网格弱化 + 面板标题条 | **0.5–1 天** | 极低，纯样式 |
| **P1** | 三栏 1px `border` 分隔 + 属性面板改自绘 key-value + 状态栏三段等宽 | **1 天** | 低（`ShowComponent` 接口不变即可） |
| **P2** | 工具栏单色图标 + 分组 + 悬停/选中态统一 + 空状态插画 | **1 天** | 低 |
| **P3** | T1 玻璃浮层 + 主题切换（dark/light 跟随系统） | **1–2 天** | 中，容易显廉价，需调参 |
| **P4** | 命令面板 `Ctrl+K`、拖拽对齐参考线 | **1.5 天** | 中，可砍 |

> 项目已进入「第 7 周起只修不加」。**建议只做 P0 + P1（1.5 天）**，P2 有余力，P3/P4 明确排到答辩之后。

---

## 八、风险清单

| 风险 | 说明 | 应对 |
| --- | --- | --- |
| **自动化断言** | 改色会打挂 `verify_mvp` / `verify_stage4` / `verify_issues14` | 改完立刻跑 4 个套件；画布色一律不动 |
| **`wxPropertyGrid` 不可主题化** | GTK 下几乎改不动 | 要 Codex 观感就得自绘属性表（P1 内） |
| **暗色 + 白画布的接缝** | 处理不好像「两个软件拼起来」 | 画布四周 8px `surface` 垫层 + 1px `border` + 极淡外阴影（T1 的用武之地） |
| **投影对比度** | 暗色界面在教室投影仪上容易发灰 | 默认 `contrast` 提到 58–65，答辩前实测 |
| **wxWidgets 3.2 限制** | 无 shader、无 backdrop blur | §五 已分级，T2 以上不进主线 |

---

## 九、验收方式

1. **截图对照**：改前/改后各截同状态 4 张（空文档 / 4 元件 3 导线 / 选中 / 仿真点亮）；
2. **断言回归**：`verify_mvp` 9/9、`verify_undo` 16/16、`verify_stage4` 15/15、`verify_issues14` 11/11 全绿 —— **改完颜色先跑 `verify_mvp`**，它最容易挂；
3. **DPI**：1.0 与 2.0 缩放下各看一次，尺寸全部 `FromDIP`；
4. **对比度**：正文对背景 ≥ 7:1，次要文字 ≥ 4.5:1。

---

## 十、明确不做的事

- ❌ **光折射 / 液态玻璃的折射效果** —— 需换 OpenGL 渲染栈，超出项目范围与工期；
- ❌ **改画布底色** —— 违反自动化断言，且违背「深色界面 + 白色工作纸」的行业惯例；
- ❌ **改几何参数**（包围盒 / 引脚长 / 命中半径 / 吸附步长） —— 与 C 的 `pinTemplate`、A 的 `addElement` 强耦合；
- ❌ **在 `ui/` 之外放 `#include <wx/...>`** —— 违反 `docs/architecture.md` 的依赖规则。
