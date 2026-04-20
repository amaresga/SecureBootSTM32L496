#!/usr/bin/env pwsh
# build.ps1 — Configure and build the project
# Usage: .\build.ps1 [-Config Debug|Release]

param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Ensure the ARM GNU toolchain is on PATH
$ArmToolchain = 'C:\Program Files (x86)\Arm\GNU Toolchain mingw-w64-i686-arm-none-eabi\bin'
if (Test-Path $ArmToolchain) {
    $env:PATH = "$ArmToolchain;$env:PATH"
} else {
    Write-Warning "ARM toolchain not found at: $ArmToolchain"
}

$Root     = $PSScriptRoot
$BuildDir = "$Root\build"   # shared for all configs (Ninja Multi-Config)

Write-Host "==> Configuring ($Config)..." -ForegroundColor Cyan
cmake -S $Root -B $BuildDir -G "Ninja Multi-Config"

Write-Host "==> Building ($Config)..." -ForegroundColor Cyan
cmake --build $BuildDir --config $Config

Write-Host "==> Build complete: $BuildDir\$Config" -ForegroundColor Green
