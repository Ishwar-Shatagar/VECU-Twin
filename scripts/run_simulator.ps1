if (Test-Path "./build/cpp/Release/vecu_sim.exe") {
    & "./build/cpp/Release/vecu_sim.exe" "./data"
} elseif (Test-Path "./build/cpp/vecu_sim.exe") {
    & "./build/cpp/vecu_sim.exe" "./data"
} else {
    Write-Error "Executable not found. Please run ./scripts/build.ps1 first."
}
