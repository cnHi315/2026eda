#!/usr/bin/env bash
# 构建并运行 MVT 测试链（编译 + ctest）。
# 运行 GUI（运行链）需要显示环境（DISPLAY 或 xvfb），见 docs/IMPLEMENTATION_ROADMAP.md。
set -e
cd "$(dirname "$0")"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure