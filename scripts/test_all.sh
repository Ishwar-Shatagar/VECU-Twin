#!/usr/bin/env bash
set -e

echo "=== 1. Running Python Tests ==="
pytest python/tests/ -v

echo "=== 2. Running C++ Tests (if built) ==="
if [ -d "./build" ]; then
    cd build && ctest --output-on-failure
    cd ..
else
    echo "Build folder not found, skipping CTest."
fi

echo "=== All Tests Passed Successfully ==="
