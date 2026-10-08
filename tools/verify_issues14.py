#!/usr/bin/env python3
"""issue 1(画布滚动/平移)+ issue 4(局部重绘/性能)的机器化验证。

issue 1:中键拖动平移后内容位移与平移量一致;平移状态下命中检测与拖动仍正确;
        网格锚定逻辑原点(平移非整格时网格线跟着平移);滚轮一格 = 一个栅格。
issue 4:拖拽只重绘受影响的小矩形(旧位置无残影);元件多时逐帧耗时可控。

用法:  python3 tools/verify_issues14.py    (需 DISPLAY / XAUTHORITY / GDK_BACKEND=x11)
"""

import ctypes
import os
import re
import subprocess
import sys
import time

from PIL import Image

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS_SRC = os.path.join(REPO, "tools", "ui_verify.cpp")
HARNESS_BIN = "/tmp/ui_verify"
SHOT_DIR = os.path.dirname(REPO)
FRAME_LOGICAL_W = 1100.0

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
    raise SystemExit("没找到窗口")


def window_size(wid):
    out = subprocess.run(["xwininfo", "-id", hex(wid)], capture_output=True,
                         text=True).stdout
    w = int([l for l in out.splitlines() if l.strip().startswith("Width")][0].split(":")[1])
    h = int([l for l in out.splitlines() if l.strip().startswith("Height")][0].split(":")[1])
    return w, h


def grab(wid, w, h):
    """抓窗口像素。注意:XImage 内存是 B,G,R,X,必须换 R/B,否则蓝色会被读成橙色。"""
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
        self.perf = []
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
            elif line.startswith("CANVAS: "):
                p = line[8:].split()
                self.canvas_screen = (int(p[0]), int(p[1]))
                self.canvas_size = (int(p[2]), int(p[3]))
                self.frame_screen = (int(p[6]), int(p[7].strip(")")))
            elif line.startswith("PERF: "):
                self.perf.append(line[6:])
            elif line.startswith("OK "):
                return

    def capture(self, name):
        img = grab(self.wid, self.w, self.h)
        img.save(os.path.join(SHOT_DIR, f"verify_i14_{name}.png"))
        return img

    def scale(self):
        return self.w / FRAME_LOGICAL_W

    def to_px(self, lx, ly):
        s = self.scale()
        ox = (self.canvas_screen[0] - self.frame_screen[0]) * s
        oy = (self.canvas_screen[1] - self.frame_screen[1]) * s
        return ox + lx * s, oy + ly * s

    def close(self):
        try:
            self.send("quit")
        except SystemExit:
            pass
        self.proc.wait(timeout=5)


def is_dark(p):
    return p[0] < 110 and p[1] < 110 and p[2] < 110


def is_major_grid(p):
    return abs(p[0] - 215) <= 4 and abs(p[1] - 215) <= 4 and abs(p[2] - 215) <= 4


def canvas_box(h, pad=420):
    """画布大致范围(像素)。wx 报的屏幕坐标在不同 DPI 下可能不准,所以先给足 pad,
    后续都用"相对同一个框"的测量比较,不依赖绝对原点。"""
    ox, oy = h.to_px(0, 0)
    s = h.scale()
    return (int(ox - pad), int(oy - pad),
            int(ox + h.canvas_size[0] * s + pad),
            int(oy + h.canvas_size[1] * s + pad))


def find_leftmost_rect(img, box):
    """在 box 内找元件矩形:深色水平边长约 120 像素(60 逻辑像素),返回最左侧那个的中心。"""
    x0b, y0b, x1b, y1b = box
    x0b, y0b = max(0, x0b), max(0, y0b)
    x1b, y1b = min(img.width, x1b), min(img.height, y1b)
    runs = []
    for y in range(y0b, y1b):
        x = x0b
        while x < x1b:
            if is_dark(img.getpixel((x, y))):
                start = x
                while x < x1b and is_dark(img.getpixel((x, y))):
                    x += 1
                if 100 <= x - start <= 160:
                    runs.append((y, start, x - 1))
            x += 1
    if not runs:
        return None
    runs.sort(key=lambda r: r[1])
    xs, xe = runs[0][1], runs[0][2]
    ys = [r[0] for r in runs if abs(r[1] - xs) < 12 and abs(r[2] - xe) < 12]
    return ((xs + xe) / 2.0, (min(ys) + max(ys)) / 2.0)


def major_grid_x(img, box):
    """box 内所有粗网格线(215 灰)的 x 像素位置。"""
    x0b, y0b, x1b, y1b = box
    x0b, y0b = max(0, x0b), max(0, y0b)
    x1b, y1b = min(img.width, x1b), min(img.height, y1b)
    out = []
    for x in range(x0b, x1b):
        n = sum(1 for y in range(y0b, y1b, 7) if is_major_grid(img.getpixel((x, y))))
        if n > (y1b - y0b) / 7 * 0.7:
            out.append(x)
    return out


results = []


def check(name, ok, detail):
    results.append(ok)
    print(f"[{'PASS' if ok else 'FAIL'}] {name}: {detail}")


def main():
    h = Harness()

    h.send("place AND 280 240")
    h.send("place LED 460 240")
    h.send("esc")

    base = h.capture("base")
    c0 = find_leftmost_rect(base, canvas_box(h))
    check("基线:找到画布上的元件矩形(U1)", c0 is not None, f"U1 像素中心 {c0}")
    # 用 U1 的实测像素位置标定画布原点(比 wx 报的屏幕坐标可靠)
    s = h.scale()
    origin = (c0[0] - s * 280.0, c0[1] - s * 240.0)
    box = (int(origin[0]), int(origin[1]),
           int(origin[0] + h.canvas_size[0] * s), int(origin[1] + h.canvas_size[1] * s))

    # ① 中键拖动平移 (+80, +40) 逻辑像素
    for cmd in ["mdown 300 300", "move 340 320", "move 380 340", "mup 380 340"]:
        h.send(cmd)
    h.send("info")
    panned = h.capture("pan")
    c1 = find_leftmost_rect(panned, box)
    d = (c1[0] - c0[0], c1[1] - c0[1]) if (c0 and c1) else None
    check("平移生效:中键拖 (+80,+40) 逻辑像素 → 内容位移 +160,+80 像素",
          d is not None and abs(d[0] - 160) <= 6 and abs(d[1] - 80) <= 6,
          f"U1 中心 {c0} -> {c1}(位移 {d})")
    check("平移状态写入状态栏", "视图已平移" in h.status0, h.status0)

    # ② 平移状态下命中检测仍准确:点 U1 的新位置
    h.send("down 360 280")
    h.send("up 360 280")
    h.send("info")
    sel = h.capture("select")
    ux, uy = h.to_px(360, 280)
    blue = 0
    for y in range(int(uy - 70), int(uy + 70)):
        for x in range(int(ux - 90), int(ux + 90)):
            p = sel.getpixel((x, y))
            if p[0] < 70 and p[1] < 140 and p[2] > 150:
                blue += 1
    check("平移后命中检测仍准:点新位置选中 U1", blue > 300 and "选中 U1" in h.status0,
          f"蓝色像素 {blue} / 状态 {h.status0}")

    # ③ 平移状态下拖动仍正确:(280,240) 拖 (+40,+20) 应落在 (320,260)
    for cmd in ["down 360 280", "move 380 290", "move 400 300", "up 400 300"]:
        h.send(cmd)
    h.send("info")
    check("平移后拖动坐标正确:落点 (320,260)", "放下 U1 位置:(320,260)" in h.status0,
          h.status0)
    dragged = h.capture("drag")
    old_spot = (int(c0[0] - 40), int(c0[1] - 30), int(c0[0] + 40), int(c0[1] + 30))
    check("局部重绘无残影:拖动后原位置已空",
          find_leftmost_rect(dragged, old_spot) is None,
          f"原位置识别 {find_leftmost_rect(dragged, old_spot)}")

    # ④ 网格锚定逻辑原点:再平移 30 逻辑像素 → 网格线整体平移 60 像素
    #    基准取"这次平移之前"的截图(粗线间距 200px,拿更早的图比会 ≡ 20 mod 200 绕圈)
    g0 = major_grid_x(dragged, box)
    for cmd in ["mdown 300 300", "move 330 300", "mup 330 300"]:
        h.send(cmd)
    g1 = major_grid_x(h.capture("grid"), box)
    matched = sum(1 for x in g0 if any(abs((x + 60) - y) <= 2 for y in g1))
    check("网格锚定逻辑原点:平移 30 逻辑像素 → 网格线整体平移 60 像素",
          len(g0) > 2 and matched >= min(len(g0), len(g1)) - 1,
          f"粗线 {len(g0)} 条 → {len(g1)} 条,匹配 {matched} 条")

    # ⑤ 滚轮滚动(一格 120 = 一个栅格 = 40 物理像素)
    before_wheel = find_leftmost_rect(h.capture("wheel_before"), box)
    h.send("wheel 300 300 120 0")
    h.send("info")
    after_wheel = find_leftmost_rect(h.capture("wheel_after"), box)
    dw = (after_wheel[0] - before_wheel[0], after_wheel[1] - before_wheel[1]) \
        if (before_wheel and after_wheel) else None
    check("滚轮滚动:一格 = 40 像素", dw is not None and abs(dw[0]) <= 4 and abs(dw[1] - 40) <= 4,
          f"位移 {dw}")

    # ⑥ issue 4:元件数量大时的逐帧耗时
    for n in (400, 1000):
        h.send(f"perf {n} 60")
        check(f"性能:放置 {n} 个元件后拖拽", bool(h.perf), h.perf[-1])
    avgs = [float(re.search(r"avg=([\d.]+)ms", t).group(1)) for t in h.perf]
    check("性能:1000 元件下平均每帧 < 40ms(>25fps)", avgs[-1] < 40.0,
          f"400 元件 {avgs[0]:.2f}ms / 1000 元件 {avgs[1]:.2f}ms 每帧")

    h.close()
    print()
    print(f"汇总:{sum(1 for r in results if r)}/{len(results)} 项通过")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
