#!/usr/bin/env python3
"""MVP 链路端到端核对(跨平台):放元件 → 连线 → 拨开关 → LED 亮。

驱动的是 tools/ui_verify.cpp 那个验证台 —— 它起真实窗口、把鼠标/键盘事件直接
注进 CanvasPanel 的事件链,所以跑的是真代码,只是绕开了操作系统的输入层。

前置:
    cmake -S . -B build -DBUILD_VERIFY_TOOLS=ON
    cmake --build build --target ui_verify

用法:
    python3 tools/verify_mvp.py build/ui_verify.exe
    (Windows 上是 build/ui_verify.exe,Linux 上是自己用 g++ 编出来的那个)

覆盖不到的部分:
    "保存 / 打开 / 导出网表"要弹原生文件对话框,这个脚本驱动不了。
    那几步的数据层由 io_test 覆盖,UI 那一段见 docs/test-plan.md 的 T-12(需人工点一次)。
"""
import os
import re
import subprocess
import sys
import tempfile
import time

# 画布坐标(逻辑像素):元件按 20 栅格吸附,引脚 = 原点 + relPos
PLACES = [
    ("SWITCH", 100, 140),   # SW1 → pins Y(+50,0) = (150,140)
    ("SWITCH", 100, 360),   # SW2 → Y = (150,360)
    ("AND", 300, 240),      # U1  → A(250,230) B(250,250) Y(350,240)
    ("LED", 500, 240),      # LED1 → A = (450,240)
]
WIRES = [((150, 140), (250, 230)),   # SW1.Y → U1.A
         ((150, 360), (250, 250)),   # SW2.Y → U1.B
         ((350, 240), (450, 240))]   # U1.Y  → LED1.A

WIRE_PX = (400, 240)    # U1.Y→LED1.A 这根线的中间
LED_PX = (500, 240)     # LED1 本体中心

ok = 0
bad = 0


def check(cond, msg):
    global ok, bad
    if cond:
        ok += 1
        print(f"  [PASS] {msg}")
    else:
        bad += 1
        print(f"  [FAIL] {msg}")


def build_script():
    lines = ["esc"]                      # 程序启动时默认在"放置 AND"模式,先退出
    for t, x, y in PLACES:
        lines.append(f"place {t} {x} {y}")
    lines.append("esc")                  # 放完退出放置模式,才能开始连线
    lines.append("info")
    for (x1, y1), (x2, y2) in WIRES:
        lines += [f"down {x1} {y1}", f"move {(x1 + x2) // 2} {(y1 + y2) // 2}", f"up {x2} {y2}"]
    lines.append("info")
    lines += [f"pixel {WIRE_PX[0]} {WIRE_PX[1]}", f"pixel {LED_PX[0]} {LED_PX[1]}"]
    for kind, x, y in PLACES:
        if kind == "SWITCH":
            lines.append(f"dclick {x} {y}")
    lines.append("info")
    lines += [f"pixel {WIRE_PX[0]} {WIRE_PX[1]}", f"pixel {LED_PX[0]} {LED_PX[1]}"]
    lines.append("quit")
    return "\n".join(lines) + "\n"


def run(exe):
    script = build_script()
    print("喂给验证台的命令:")
    print("  " + script.replace("\n", "\n  ").rstrip())

    # 不用 capture_output:wx 的 GUI 程序在 Windows 的 CreateProcess 管道下拿不到 stdout,
    # 但重定向到文件是好的。所以走临时文件。
    t0 = time.monotonic()
    with tempfile.TemporaryDirectory() as td:
        in_path = os.path.join(td, "cmds.txt")
        out_path = os.path.join(td, "out.txt")
        with open(in_path, "w", encoding="utf-8") as f:
            f.write(script)
        try:
            with open(in_path, encoding="utf-8") as fi, open(out_path, "w", encoding="utf-8") as fo:
                rc = subprocess.call([exe], stdin=fi, stdout=fo,
                                     stderr=subprocess.STDOUT, timeout=120)
        except OSError as e:
            print(f"启动验证台失败:{e}")
            print("最常见的原因是 exe 找不到 MinGW 的运行时 DLL")
            print("(libgcc_s_seh-1.dll / libstdc++-6.dll)—— Windows 会把它报成「找不到文件」。")
            print("把 MinGW 的 bin 目录加进 PATH,或把这两个 DLL 放到 exe 旁边即可。")
            return 2
        with open(out_path, encoding="utf-8", errors="replace") as f:
            out = f.read()

    status = re.findall(r"STATUS1: 元件 (\d+) \| 导线 (\d+) \| 网络 (\d+)", out)
    pixels = re.findall(r"PIXEL: (\d+) (\d+) (\d+) (\d+) (\d+)", out)
    canvas = re.search(r"CANVAS: \d+ \d+ (\d+) (\d+)", out)

    # 坐标是按 Windows 上 600x594 的画布定的;别的平台布局可能不同,先确认画布够大,
    # 否则后面的断言会以"放不上去"的形式莫名其妙地失败。
    print("\n== 0. 画布尺寸 ==")
    if canvas:
        cw, ch = int(canvas.group(1)), int(canvas.group(2))
        check(cw >= 540 and ch >= 400, f"画布 {cw}x{ch} 够放下这套坐标(需要 ≥540x400)")
    else:
        check(False, "没读到 CANVAS 尺寸")

    print("\n== 1. 放置与连线 ==")
    check(rc == 0, f"验证台正常退出(returncode={rc})")
    # 一共 info 三次:放完 / 连完 / 拨完
    check(len(status) >= 3, f"读到状态栏计数({len(status)} 次)")
    if len(status) >= 3:
        check(status[0] == ("4", "0", "0"), f"放完 4 个元件 → 元件/导线/网络 = {status[0]}")
        check(status[1] == ("4", "3", "3"), f"连完 3 根线 → 元件/导线/网络 = {status[1]}")
        check(status[2] == status[1], f"拨开关不改变计数 = {status[2]}")

    if len(pixels) < 4:
        # wxGTK 上 wxDC::GetPixel 不一定实现(会打印 "n/a"),此时跳过视觉断言,
        # 状态栏那几项仍然有效。Linux 上的像素核对继续走 verify_stage4.py 的 XGetImage。
        print("\n== 2/3. 视觉断言:跳过 ==")
        print("  读不到画布像素(该平台的 wxDC::GetPixel 未实现)—— 状态栏断言已覆盖结构部分")
        print(f"\n---- {ok}/{ok + bad} 通过(视觉断言跳过)----")
        return 0 if bad == 0 else 1

    print("\n== 2. 拨开关前 ==")
    _, _, r, g, b = map(int, pixels[1])
    check(r > 240 and g > 240 and b > 240, f"LED 是灭的(白色):rgb=({r},{g},{b})")

    print("\n== 3. 双击两个开关之后 ==")
    _, _, r, g, b = map(int, pixels[2])
    check(g > 150 and r < 60 and b < 60, f"导线变亮绿(高电平):rgb=({r},{g},{b})")
    _, _, r, g, b = map(int, pixels[3])
    check(r > 200 and 100 < g < 220 and b < 60, f"LED 点亮(橙色):rgb=({r},{g},{b})")

    print(f"\n整轮耗时 {time.monotonic() - t0:.1f}s(自动化,不含人工操作时间)")
    print(f"---- {ok}/{ok + bad} 通过 ----")
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    default_exe = "build/ui_verify.exe"
    if not os.path.exists(default_exe) and os.path.exists("build/ui_verify"):
        default_exe = "build/ui_verify"        # Linux 上没有 .exe 后缀
    exe_path = sys.argv[1] if len(sys.argv) > 1 else default_exe
    # Windows 上 subprocess 用相对路径(尤其是带 / 的)会找不到文件,统一转绝对路径
    sys.exit(run(os.path.abspath(exe_path)))
