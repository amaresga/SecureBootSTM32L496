#!/usr/bin/env pwsh
# flash.ps1 — Flash bootloader and/or application onto the board
# Usage: .\flash.ps1 [-Config Debug|Release] [-Target bootloader|app|all] [-Port SWD|JTAG] [-Freq 4000]
#   -Target  Which image to flash (default: all)

param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [ValidateSet('bootloader','app','all')]
    [string]$Target = 'all',

    [ValidateSet('SWD','JTAG')]
    [string]$Port = 'SWD',

    [int]$Freq = 4000        # kHz
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Locate STM32CubeProgrammer — try known install paths, then fall back to PATH
$CubeProgCandidates = @(
    'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin',
    'C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin',
    "$env:LOCALAPPDATA\Programs\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin"
)
foreach ($candidate in $CubeProgCandidates) {
    if (Test-Path "$candidate\STM32_Programmer_CLI.exe") {
        $env:PATH = "$candidate;$env:PATH"
        break
    }
}

$Root       = $PSScriptRoot
$Programmer = "STM32_Programmer_CLI.exe"

# Map target name to ELF path (Ninja Multi-Config layout: build/<subdir>/<Config>/<name>.elf)
$elfMap = @{
    'bootloader' = "$Root\build\bootloader\$Config\Bootloader.elf"
    'app'        = "$Root\build\app\$Config\App.elf"
}
$targets = if ($Target -eq 'all') { @('bootloader', 'app') } else { @($Target) }

# Verify the programmer is reachable
if (-not (Get-Command $Programmer -ErrorAction SilentlyContinue)) {
    Write-Error "'$Programmer' not found on PATH. Add STM32CubeProgrammer\bin to your PATH."
}

# Verify all requested ELF files exist before touching the board
foreach ($t in $targets) {
    $elf = $elfMap[$t]
    if (-not (Test-Path $elf)) {
        Write-Error "ELF not found: $elf`nRun .\build.ps1 -Config $Config -Target $t first."
    }
}

# Flash each target in order (bootloader first, then app)
foreach ($t in $targets) {
    $elf = $elfMap[$t]
    Write-Host "==> Flashing $t ($Config) via $Port @ $Freq kHz..." -ForegroundColor Cyan
    Write-Host "    ELF: $elf" -ForegroundColor Gray

    # -c  : connect   (port, frequency, reset=HWrst for a clean connect)
    # -w  : write ELF to target
    # -v  : verify after write
    # Only reset and run after the last image is flashed
    $isLast = ($t -eq $targets[-1])
    if ($isLast) {
        & $Programmer -c port=$Port freq=$Freq reset=HWrst -w $elf -v -rst
    } else {
        & $Programmer -c port=$Port freq=$Freq reset=HWrst -w $elf -v
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "STM32_Programmer_CLI exited with code $LASTEXITCODE while flashing $t"
    }
    Write-Host "    Done." -ForegroundColor Green
}

Write-Host "==> Flash complete. Board is running." -ForegroundColor Green
