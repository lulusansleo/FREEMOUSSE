#!/usr/bin/env bash
set -e
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build --parallel
echo ""
echo "Binary: ./build/dj_visualizer"
