#!/usr/bin/env python3
"""阶段四(真数据上屏)端到端验证。

驱动 tools/ui_verify.cpp 起的真实 UiMainFrame,按脚本:
  放置 4 个元件 → 连 3 条线 → 选中高亮 → 双击两个开关 → 混合输入 → 拖动 → 删除,
每步核对状态栏文本,并用 XGetImage 抓窗口像素核对"线变亮绿 / LED 变亮 / 开关底色"。

用法:  python3 tools/verify_stage4.py     (需 DISPLAY / XAUTHORITY / GDK_BACKEND=x11)
"""

import ctypes
import os
import subprocess
import sys
import time

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS_SRC = os.path.join(REPO, "tools", "ui_verify.cpp")
HARNESS_BIN = "/tmp/ui_verify"
SHOT_DIR = os.path.dirname(REPO)          # 截图写到仓库上一级,别污染仓库
FRAME_LOGICAL_W = 1100.0                  # UiMainFrame 的逻辑宽度

X11 = ctypes.CDLL("libX11.so.6")
X11.XOpenDisplay.restype = ctypes.c_void_p
DPY = X11.XOpenDisplay(b":0")


class XImage(ctypes.Structure):
    _fields_ = [
        ("width", ctypes.c_int), ("height", ctypes.c_int),
        ("xoffset", ctypes.c_int), ("format", ctypes.c_int),
        ("data", ctypes.c_void_p), ("byte_order", ctypes.c_int),
        ("bitmap_unit", ctypes.c_int), ("bitmap_bit_order", ctypes.c_int),
        ("bitmap_pad", ctypes.c_int), ("depth", ctypes.c_int),
        ("bytes_per_line", ctypes.c_int), ("bits_per_pixel", ctypes.c_int),
        ("red_mask", ctypes.c_ulong), ("green_mask", ctypes.c_ulong),
        ("blue_mask", ctypes.c_ulong),
    ]


X11.XGetImage.restype = ctypes.POINTER(XImage)


def build_harness():
    """源码有更新就重新编译验证台(不需要动仓库的 CMake)。"""
    newest = max(os.path.getmtime(os.path.join(d, f))
                 for d, _, fs in os.walk(os.path.join(REPO, "src")) for f in fs)
    fresh = (os.path.exists(HARNESS_BIN)
             and os.path.getmtime(HARNESS_BIN) > newest
             and os.path.getmtime(HARNESS_BIN) > os.path.getmtime(HARNESS_SRC))
    if fresh:
        return
    cxxflags = subprocess.run(["wx-config", "--cxxflags"], capture_output=True,
                              text=True).stdout.split()
    libs = subprocess.run(["wx-config", "--libs", "core,base,propgrid"],
                          capture_output=True, text=True).stdout.split()
    sources = [os.path.join(REPO, "src/ui", f"{n}.cpp") for n in
               ("canvas_panel", "component_palette", "main_frame", "property_panel")]
    sources += [os.path.join(REPO, "src/model/schematic_model.cpp"),
                os.path.join(REPO, "src/components/component_library.cpp"),
                os.path.join(REPO, "src/io/netlist_io.cpp"),
                os.path.join(REPO, "src/simulation/simulator.cpp")]
    print("构建验证台 …")
    subprocess.run(["g++", "-std=c++17", "-I", os.path.join(REPO, "src"), *cxxflags,
                    HARNESS_SRC, *sources, *libs, "-o", HARNESS_BIN], check=True)


def find_window(title_part):
    for _ in range(40):
        out = subprocess.run(["xwininfo", "-root", "-tree"], capture_output=True,
                             text=True).stdout
        for line in out.splitlines():
            if title_part in line and "mutter" not in line:
                return int(line.split()[0], 16)
        time.sleep(0.25)
    raise SystemExit(f"没找到窗口:{title_part}")


def window_size(wid):
    out = subprocess.run(["xwininfo", "-id", hex(wid)], capture_output=True,
                         text=True).stdout
    w = int([l for l in out.splitlines() if l.strip().startswith("Width")][0].split(":")[1])
    h = int([l for l in out.splitlines() if l.strip().startswith("Height")][0].split(":")[1])
    return w, h


def grab(wid, w, h):
    img = X11.XGetImage(ctypes.c_void_p(DPY), ctypes.c_ulong(wid), 0, 0, w, h,
                        ctypes.c_ulong(0xFFFFFFFF), 2)
    ic = img.contents
    raw = ctypes.string_at(ic.data, ic.bytes_per_line * ic.height)
    full = Image.frombytes("RGBX", (ic.bytes_per_line // 4, ic.height), raw)
    r, g, b, _ = full.split()
    return Image.merge("RGB", (b, g, r)).crop((0, 0, ic.width, ic.height))


class Harness:
    def __init__(self):
        build_harness()
        self.proc = subprocess.Popen([HARNESS_BIN], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1,
                                     env=os.environ.copy())
        self.wid = find_window("Circuit Editor")
        self.w, self.h = window_size(self.wid)
        self.status0 = ""
        self.status1 = ""
        self.title = ""
        time.sleep(1.0)
        self.send("info")

    def send(self, cmd):
        self.proc.stdin.write(cmd + "\n")
        self.proc.stdin.flush()
        while True:
            line = self.proc.stdout.readline()
            if not line:
                raise SystemExit("验证台意外退出")
            line = line.rstrip("\n")
            if line.startswith("STATUS0: "):
                self.status0 = line[9:]
            elif line.startswith("STATUS1: "):
                self.status1 = line[9:]
            elif line.startswith("TITLE: "):
                self.title = line[7:]
            elif line.startswith("CANVAS: "):
                p = line[8:].split()
                self.canvas_screen = (int(p[0]), int(p[1]))
                self.canvas_size = (int(p[2]), int(p[3]))
                self.frame_screen = (int(p[6]), int(p[7].strip(")")))
            elif line.startswith("OK "):
                return

    def capture(self, name):
        img = grab(self.wid, self.w, self.h)
        img.save(os.path.join(SHOT_DIR, f"verify_s4_{name}.png"))
        return img

    def canvas_rect(self):
        """画布在窗口图像里的像素矩形(物理):粗算即可,脚本只用它限定扫描范围。"""
        s = self.w / FRAME_LOGICAL_W
        ox = (self.canvas_screen[0] - self.frame_screen[0]) * s
        oy = (self.canvas_screen[1] - self.frame_screen[1]) * s
        return ox, oy, ox + self.canvas_size[0] * s, oy + self.canvas_size[1] * s

    def close(self):
        try:
            self.send("quit")
        except SystemExit:
            pass
        self.proc.wait(timeout=5)


def count(img, pred, region):
    x0, y0, x1, y1 = (int(v) for v in region)
    x0, y0 = max(0, x0), max(0, y0)
    x1, y1 = min(img.width, x1), min(img.height, y1)
    n = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            if pred(img.getpixel((x, y))):
                n += 1
    return n


results = []


def check(name, ok, detail):
    results.append(ok)
    print(f"[{'PASS' if ok else 'FAIL'}] {name}: {detail}")


def main():
    h = Harness()
    scale = h.w / FRAME_LOGICAL_W
    ox, oy, x1, y1 = h.canvas_rect()
    region = (ox, oy, x1, y1)

    def to_px(lx, ly):
        return ox + lx * scale, oy + ly * scale

    # ① 放置 4 个元件(位置必须落在 20 的倍数上,否则会被吸附)
    for cmd in ["place SWITCH 100 160", "place SWITCH 100 300",
                "place AND 280 240", "place LED 460 240"]:
        h.send(cmd)
    h.send("info")
    check("放置 4 个元件:模型计数正确", "元件 4 | 导线 0 | 网络 0" in h.status1, h.status1)
    check("放置后状态栏报告最后一个元件", "已放置 LED1" in h.status0, h.status0)
    h.send("esc")                        # 退出放置模式,免得后面点空白误放元件

    # ② 连 3 条线:SW1.Y(150,160)→U1.A(230,230)、SW2.Y(150,300)→U1.B(230,250)、U1.Y(330,240)→LED1.A(410,240)
    for cmd in ["down 150 160", "move 190 190", "move 230 230", "up 230 230",
                "down 150 300", "move 190 270", "move 230 250", "up 230 250",
                "down 330 240", "move 370 240", "up 410 240"]:
        h.send(cmd)
    h.send("info")
    check("连线 3 条:模型生成 3 个网络", "元件 4 | 导线 3 | 网络 3" in h.status1, h.status1)
    check("连线状态栏给出网络 id",
          "已连线 U1#2 → LED1#0" in h.status0 and "net" in h.status0, h.status0)

    def bright_green(p):
        return p[0] < 60 and p[1] > 150 and p[2] < 60

    def sel_blue(p):
        return p[0] < 70 and p[1] < 140 and p[2] > 150

    def led_orange(p):
        return p[0] > 220 and 140 < p[1] < 200 and p[2] < 60

    def switch_tint(p):
        return 200 < p[0] < 225 and 235 < p[1] < 255 and 200 < p[2] < 225

    before = h.capture("before")
    g0 = count(before, bright_green, region)
    l0 = count(before, led_orange, region)
    s0 = count(before, switch_tint, region)
    print(f"       开关都关:亮绿 {g0} / LED 橙 {l0} / 开关底色 {s0}")

    # ③ 选中高亮(阶段三行为在真数据下的回归)
    h.send("down 280 240")
    h.send("up 280 240")
    sel = h.capture("select")
    ux, uy = to_px(280, 240)
    u1_region = (ux - 45 * scale, uy - 35 * scale, ux + 45 * scale, uy + 35 * scale)
    blue = count(sel, sel_blue, u1_region)
    check("选中高亮:点 U1 出现蓝色外框(阶段三回归)", blue > 300, f"蓝色像素 {blue}")
    h.send("down 60 500")
    h.send("up 60 500")

    # ④ 双击两个开关 → 都打开
    h.send("dclick 100 160")
    h.send("dclick 100 300")
    h.send("info")
    check("双击拨开关:状态栏报告打开", "开关 SW2 打开(1)" in h.status0, h.status0)
    check("拨开关不改变元件/连线计数", "元件 4 | 导线 3 | 网络 3" in h.status1, h.status1)

    after = h.capture("after")
    g1 = count(after, bright_green, region)
    l1 = count(after, led_orange, region)
    s1 = count(after, switch_tint, region)
    check("仿真上屏:导线按电平变亮绿", g1 > g0 + 400, f"亮绿像素 {g0} -> {g1}")
    check("仿真上屏:LED 点亮(橙色实心)", l1 > l0 + 80, f"LED 橙像素 {l0} -> {l1}")
    check("仿真上屏:开关闭合(底色变浅绿)", s1 > s0 + 300, f"开关底色 {s0} -> {s1}")

    # ⑤ 混合输入:SW1 拨回关闭(SW1=0, SW2=1)→ 与门输出 0、LED 灭(T-07)
    h.send("dclick 100 160")
    h.send("info")
    check("混合输入:SW1 关闭", "开关 SW1 关闭(0)" in h.status0, h.status0)
    mixed = h.capture("mixed")
    g2 = count(mixed, bright_green, region)
    l2 = count(mixed, led_orange, region)
    check("仿真上屏:SW1=0, SW2=1 → LED 灭", l2 < 100, f"LED 橙像素 {l1} -> {l2}")
    check("仿真上屏:该网络回到低电平(不亮绿)", g2 < g1 - 200, f"亮绿像素 {g1} -> {g2}")
    h.send("dclick 100 160")

    # ⑥ 拖动元件:连线与计数不变
    for cmd in ["down 460 240", "move 480 250", "move 520 280", "up 520 280"]:
        h.send(cmd)
    h.send("info")
    check("拖拽走 model.moveElement:计数不变",
          "元件 4 | 导线 3 | 网络 3" in h.status1 and "放下 LED1 位置:(520,280)" in h.status0,
          f"{h.status0} | {h.status1}")

    # ⑦ 删除:选中 LED1 → Del
    h.send("down 520 280")
    h.send("up 520 280")
    h.send("del")
    h.send("info")
    check("删除元件:连线一并断开", "元件 3 | 导线 2 | 网络 2" in h.status1, h.status1)
    h.capture("delete")

    h.close()
    print()
    print(f"汇总:{sum(1 for r in results if r)}/{len(results)} 项通过")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
