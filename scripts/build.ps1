Write-Host "=== Building C++ Simulator and Test Suites ===" -ForegroundColor Cyan
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
Write-Host "=== Build Complete ===" -ForegroundColor Green
