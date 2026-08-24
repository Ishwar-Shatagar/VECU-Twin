#!/usr/bin/env bash
set -e

if [ -f "./build/cpp/vecu_sim" ]; then
    ./build/cpp/vecu_sim ./data
elif [ -f "./build/cpp/Release/vecu_sim.exe" ]; then
    ./build/cpp/Release/vecu_sim.exe ./data
else
    echo "Executable not found. Please run ./scripts/build.sh first."
    exit 1
fi
