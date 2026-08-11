# Measures the build speed (Windows): clean configure + full build.
# Usage: powershell -File tools/bench_build.ps1 [-Standard 17|20|23]
param(
    [string]$Generator = "MinGW Makefiles",
    [int]$Standard = 17
)
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root "bench-build"

Write-Host "Cleaning $buildDir ..."
if (Test-Path $buildDir) {
    Remove-Item $buildDir -Recurse -Force
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host "Configure (C++$Standard, Release) ..."
cmake -S $root -B $buildDir -G $Generator `
    -DCMAKE_BUILD_TYPE=Release -DLOGGER_CXX_STANDARD=$Standard
if (-not $?) { exit 1 }

Write-Host "Build ..."
cmake --build $buildDir -j
if (-not $?) { exit 1 }

$stopwatch.Stop()
$size = (Get-ChildItem $buildDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
Write-Host ""
Write-Host ("Build time : {0:N1} s" -f $stopwatch.Elapsed.TotalSeconds)
Write-Host ("C++ std    : {0}" -f $Standard)
Write-Host ("Generator  : {0}" -f $Generator)
Write-Host ("Output     : {0:N1} MB" -f ($size / 1MB))