#!/usr/bin/env python3
"""撤销 / 重做(F-06,快照式)机器化验证。

覆盖:
  放置 / 连线 / 删除 三类编辑的撤销与重做;
  单击选中、Esc 还原、拨开关**不入栈**(不会污染历史);
  新建文档会清空历史。

用法:  python3 tools/verify_undo.py    (需 DISPLAY / XAUTHORITY / GDK_BACKEND=x11)
"""

import os
import subprocess
import sys
import time

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HARNESS_SRC = os.path.join(REPO, "tools", "ui_verify.cpp")
HARNESS_BIN = "/tmp/ui_verify"


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
               ("canvas_panel", "component_palette", "main_frame", "property_panel", "subcircuit_library")]
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


class Harness:
    def __init__(self):
        build_harness()
        self.proc = subprocess.Popen([HARNESS_BIN], stdin=subprocess.PIPE,
                                     stdout=subprocess.PIPE, text=True, bufsize=1,
                                     env=os.environ.copy())
        self.wid = find_window("Circuit Editor")
        self.status0 = ""
        self.status1 = ""
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
            elif line.startswith("OK "):
                return

    def counts(self):
        self.send("info")
        return self.status1

    def close(self):
        try:
            self.send("quit")
        except SystemExit:
            pass
        self.proc.wait(timeout=5)


results = []


def check(name, ok, detail):
    results.append(ok)
    print(f"[{'PASS' if ok else 'FAIL'}] {name}: {detail}")


def main():
    h = Harness()
    h.send("new")

    # ① 放置 3 个元件 → 连续撤销 3 次 → 连续重做 3 次
    for cmd in ["place SWITCH 100 160", "place AND 280 240", "place LED 460 240"]:
        h.send(cmd)
    h.send("esc")
    check("放置 3 个元件", "元件 3 | 导线 0 | 网络 0" in h.counts(), h.status1)

    for _ in range(3):
        h.send("undo")
    check("撤销 3 次回到空文档", "元件 0 | 导线 0 | 网络 0" in h.counts() and
          "已撤销" in h.status0, f"{h.status1} / {h.status0}")

    for _ in range(3):
        h.send("redo")
    check("重做 3 次恢复 3 个元件", "元件 3 | 导线 0 | 网络 0" in h.counts() and
          "已重做" in h.status0, f"{h.status1} / {h.status0}")

    # ② 连线:撤销 / 重做
    for cmd in ["down 150 160", "move 190 190", "move 230 230", "up 230 230"]:
        h.send(cmd)
    check("连线后 1 条导线 1 个网络", "元件 3 | 导线 1 | 网络 1" in h.counts(), h.status1)
    h.send("undo")
    check("撤销连线", "元件 3 | 导线 0 | 网络 0" in h.counts(), h.status1)
    h.send("redo")
    check("重做连线", "元件 3 | 导线 1 | 网络 1" in h.counts(), h.status1)

    # ③ 单击选中不入栈:单击后撤销,应回到"连线之前"
    h.send("down 280 240")
    h.send("up 280 240")
    h.send("info")
    check("单击选中(状态栏报告选中)", "选中 U1" in h.status0, h.status0)
    h.send("undo")
    check("单击不污染历史:撤销回到连线前", "元件 3 | 导线 0 | 网络 0" in h.counts(), h.status1)
    h.send("redo")                                  # 把连线放回来

    # ④ 删除元件 → 撤销恢复
    #    注意:此时还剩一条 SW1.Y→U1.A 的导线,删 LED1 不会动它
    h.send("down 460 240")
    h.send("up 460 240")
    h.send("del")
    check("删除 LED1", "元件 2 | 导线 1 | 网络 1" in h.counts(), h.status1)
    h.send("undo")
    check("撤销删除", "元件 3 | 导线 1 | 网络 1" in h.counts(), h.status1)

    # ⑤ 拖动 → 撤销后位置还原(用"能否在新/旧位置选中"判断)
    for cmd in ["down 280 240", "move 320 260", "move 360 280", "up 360 280"]:
        h.send(cmd)
    h.send("info")
    check("拖动 U1 到 (360,280)", "放下 U1 位置:(360,280)" in h.status0, h.status0)
    h.send("undo")
    h.send("down 280 240")
    h.send("up 280 240")
    h.send("info")
    check("撤销拖动:旧位置能选中 U1", "选中 U1" in h.status0, h.status0)
    h.send("down 360 280")
    h.send("up 360 280")
    h.send("info")
    check("撤销拖动:新位置已无元件", "选中" not in h.status0, h.status0)

    # ⑥ 拨开关不入栈:新建 → 放一个开关 → 拨动 → 撤销(应撤销"放置")
    h.send("new")
    h.send("place SWITCH 100 160")
    h.send("esc")
    h.send("dclick 100 160")
    h.send("info")
    check("放置并拨动开关", "元件 1" in h.status1 and "开关 SW1 打开(1)" in h.status0,
          f"{h.status1} / {h.status0}")
    h.send("undo")
    check("拨开关不入栈:撤销直接回到空文档", "元件 0 | 导线 0 | 网络 0" in h.counts(),
          h.status1)

    # ⑦ 新建文档会清空历史
    h.send("undo")
    h.send("info")
    check("历史已清空:再撤销提示「没有可撤销的操作」",
          "没有可撤销的操作" in h.status0, h.status0)

    h.close()
    print()
    print(f"汇总:{sum(1 for r in results if r)}/{len(results)} 项通过")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
