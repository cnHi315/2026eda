#!/usr/bin/env bash
# Linux 一键构建(Windows 见 README 第四节)。
set -e
cd "$(dirname "$0")"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
echo "产物: build/CircuitEditor(运行需要图形环境)"