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

# Map target to flash image.
# Bootloader: ELF (no post-build patching needed).
# App:        HEX (patch_image.py patches length+CRC into .bin then regenerates .hex;
#             the .elf still has the unpatched zeros and must NOT be flashed directly).
$imageMap = @{
    'bootloader' = "$Root\build\bootloader\$Config\Bootloader.elf"
    'app'        = "$Root\build\app\$Config\App.hex"
}
$targets = if ($Target -eq 'all') { @('bootloader', 'app') } else { @($Target) }

# Verify the programmer is reachable
if (-not (Get-Command $Programmer -ErrorAction SilentlyContinue)) {
    Write-Error "'$Programmer' not found on PATH. Add STM32CubeProgrammer\bin to your PATH."
}

# Verify all requested image files exist before touching the board
foreach ($t in $targets) {
    $img = $imageMap[$t]
    if (-not (Test-Path $img)) {
        Write-Error "Image not found: $img`nRun .\build.ps1 -Config $Config -Target $t first."
    }
}

# Flash each target in order (bootloader first, then app)
foreach ($t in $targets) {
    $img = $imageMap[$t]
    Write-Host "==> Flashing $t ($Config) via $Port @ $Freq kHz..." -ForegroundColor Cyan
    Write-Host "    Image: $img" -ForegroundColor Gray

    # -c  : connect   (port, frequency, reset=HWrst for a clean connect)
    # -w  : write image to target
    # -v  : verify after write
    # Only reset and run after the last image is flashed
    $isLast = ($t -eq $targets[-1])
    if ($isLast) {
        & $Programmer -c port=$Port freq=$Freq reset=HWrst -w $img -v -rst
    } else {
        & $Programmer -c port=$Port freq=$Freq reset=HWrst -w $img -v
    }

    if ($LASTEXITCODE -ne 0) {
        Write-Error "STM32_Programmer_CLI exited with code $LASTEXITCODE while flashing $t"
    }
    Write-Host "    Done." -ForegroundColor Green
}

Write-Host "==> Flash complete. Board is running." -ForegroundColor Green
