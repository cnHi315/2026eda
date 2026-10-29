"""Check whether the GUI benchmark reports the actual circuit size.

Run with DISPLAY/XAUTHORITY/GDK_BACKEND set, passing the ui_verify executable.
Exit 2 means a known reporting bug was reproduced; 1 means the probe failed.
"""
from pathlib import Path
import re
import subprocess
import sys


def main():
    if len(sys.argv) != 2:
        print("Usage: python3 tests/b_perf_count_probe.py <ui_verify>")
        return 1
    commands = "\n".join([
        "new", "place AND 280 240", "place LED 460 240", "esc",
        "perf 2 60", "info", "perf 3 60", "info", "quit", "",
    ])
    try:
        result = subprocess.run([str(Path(sys.argv[1]).resolve())], input=commands,
                                capture_output=True, text=True, timeout=30)
    except (OSError, subprocess.TimeoutExpired) as error:
        print(f"[FAIL] benchmark probe could not complete: {error}")
        return 1
    print(result.stdout)
    if result.stderr:
        print(result.stderr, file=sys.stderr)
    reports = re.findall(r"PERF: n=(\d+) actual=(\d+)", result.stdout)
    labels = [label for label, count in reports]
    actual = re.findall(r"STATUS1: 元件 (\d+) \|", result.stdout)
    if result.returncode or len(labels) != 2 or len(actual) != 2:
        print("[FAIL] missing benchmark results or model counts")
        return 1
    discrepancies = [(int(label), int(count)) for (label, report), count in zip(reports, actual)
                     if label != count or report != count]
    for label, count in discrepancies:
        print(f"[BUG] B-BUG-006 benchmark label={label}, actual component count={count}")
    if discrepancies:
        return 2
    print("[PASS] benchmark labels match actual component counts")
    return 0


if __name__ == "__main__":
    sys.exit(main())
