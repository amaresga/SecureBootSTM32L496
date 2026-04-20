#!/usr/bin/env pwsh
# flash.ps1 — Flash the built ELF onto the board via STM32CubeProgrammer CLI
# Usage: .\flash.ps1 [-Config Debug|Release] [-Port SWD|JTAG] [-Freq 4000]

param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

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
$ElfFile    = "$Root\build\$Config\STM32L496_Boot.elf"
$Programmer = "STM32_Programmer_CLI.exe"

# Verify the programmer is reachable
if (-not (Get-Command $Programmer -ErrorAction SilentlyContinue)) {
    Write-Error "'$Programmer' not found on PATH. Add STM32CubeProgrammer\bin to your PATH."
}

# Verify the build output exists
if (-not (Test-Path $ElfFile)) {
    Write-Error "ELF not found: $ElfFile`nRun .\build.ps1 -Config $Config first."
}

Write-Host "==> Connecting via $Port @ $Freq kHz..." -ForegroundColor Cyan
Write-Host "    ELF : $ElfFile" -ForegroundColor Gray

# -c  : connect   (port, frequency, reset=HWrst for a clean connect)
# -w  : write ELF/HEX/BIN to target
# -v  : verify after write
# -rst: reset and run after flashing
& $Programmer -c port=$Port freq=$Freq reset=HWrst -w $ElfFile -v -rst

if ($LASTEXITCODE -ne 0) {
    Write-Error "STM32_Programmer_CLI exited with code $LASTEXITCODE"
}

Write-Host "==> Flash complete. Board is running." -ForegroundColor Green
