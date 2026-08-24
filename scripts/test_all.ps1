Write-Host "=== 1. Running Python Tests ===" -ForegroundColor Cyan
pytest python/tests/ -v

if (Test-Path "./build") {
    Write-Host "=== 2. Running C++ Tests ===" -ForegroundColor Cyan
    Set-Location "./build"
    ctest --output-on-failure
    Set-Location ".."
}

Write-Host "=== All Tests Complete ===" -ForegroundColor Green
