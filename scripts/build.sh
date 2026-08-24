#!/usr/bin/env bash
set -e

echo "=== Building C++ Simulator and Test Suites ==="
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release

echo "=== Build Complete ==="
