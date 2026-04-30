#!/usr/bin/env pwsh
# build.ps1 - Configure and build the secure-boot project
# Usage: .\build.ps1 [-Config Debug|Release] [-Target bootloader|app|all] [-Clean] [-Sign]
#   -Config   Build configuration (default: Debug)
#   -Target   Which target(s) to build (default: all)
#   -Clean    Delete the build directory before configuring
#   -Sign     Sign the app image after building (requires SIGNING_PASSPHRASE env var or interactive prompt)

param(
    [ValidateSet('Debug','Release')]
    [string]$Config = 'Debug',

    [ValidateSet('bootloader','app','all')]
    [string]$Target = 'all',

    [switch]$Clean,
    [switch]$Sign
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

# Load .env file if present (local dev secrets - gitignored, never committed)
# Supports KEY=VALUE lines; lines starting with # are comments.
$EnvFile = "$Root\.env"
if (Test-Path $EnvFile) {
    Get-Content $EnvFile | ForEach-Object {
        $line = $_.Trim()
        if ($line -and -not $line.StartsWith('#')) {
            $idx = $line.IndexOf('=')
            if ($idx -gt 0) {
                $varName  = $line.Substring(0, $idx).Trim()
                $varValue = $line.Substring($idx + 1).Trim()
                [System.Environment]::SetEnvironmentVariable($varName, $varValue, 'Process')
            }
        }
    }
    Write-Host "==> Loaded .env" -ForegroundColor DarkGray
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "==> Cleaning $BuildDir..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
    Write-Host "==> Clean complete." -ForegroundColor Green
}

Write-Host "==> Configuring ($Config)..." -ForegroundColor Cyan
cmake -S $Root -B $BuildDir -G "Ninja Multi-Config"

# Resolve CMake target name(s)
$cmakeTargets = switch ($Target) {
    'bootloader' { @('Bootloader.elf') }
    'app'        { @('App.elf') }
    'all'        { @('Bootloader.elf', 'App.elf') }
}

foreach ($t in $cmakeTargets) {
    Write-Host "==> Building $t ($Config)..." -ForegroundColor Cyan
    cmake --build $BuildDir --config $Config --target $t
}

Write-Host "==> Build complete: $BuildDir\$Config" -ForegroundColor Green

if ($Sign -and $Target -in @('app', 'all')) {
    $AppBin    = "$BuildDir\app\$Config\App.bin"
    $SignScript = "$Root\tools\sign_image.py"
    $Python    = "$Root\.venv\Scripts\python.exe"

    if (-not (Test-Path $AppBin)) {
        Write-Error "App binary not found: $AppBin"
    }
    if (-not (Test-Path $Python)) {
        Write-Error "Python venv not found at: $Python - run: uv venv .venv; uv pip install --python .venv\Scripts\python.exe -r tools\requirements.txt"
    }

    # Resolve key path - three sources in priority order:
    #   1. SIGNING_PRIVATE_KEY env var  - CI: PEM file *contents* as a masked secret
    #   2. SIGNING_KEY_PATH env var     - local dev: path to PEM outside the repo
    #   3. tools/private_key.pem        - fallback (not recommended for long-term use)
    $TempKey = $null
    if ($env:SIGNING_PRIVATE_KEY) {
        # CI mode: write PEM contents to a temp file, clean up in finally block
        $TempKey = [System.IO.Path]::Combine([System.IO.Path]::GetTempPath(), [System.IO.Path]::GetRandomFileName() + '.pem')
        [System.IO.File]::WriteAllText($TempKey, $env:SIGNING_PRIVATE_KEY)
        $KeyArg = $TempKey
        Write-Host "==> Using SIGNING_PRIVATE_KEY (CI mode)" -ForegroundColor DarkGray
    } elseif ($env:SIGNING_KEY_PATH) {
        $KeyArg = $env:SIGNING_KEY_PATH
        if (-not (Test-Path $KeyArg)) { Write-Error "SIGNING_KEY_PATH not found: $KeyArg" }
        Write-Host "==> Using key: $KeyArg" -ForegroundColor DarkGray
    } else {
        $KeyArg = "$Root\tools\private_key.pem"
        Write-Warning "SIGNING_KEY_PATH not set - falling back to tools/private_key.pem (store the key outside the repo for production use)"
    }

    try {
        Write-Host "==> Signing $AppBin..." -ForegroundColor Cyan
        & $Python $SignScript $AppBin --key $KeyArg
        if ($LASTEXITCODE -ne 0) { throw "sign_image.py failed (exit $LASTEXITCODE)" }
        Write-Host "==> Signing complete." -ForegroundColor Green
    } finally {
        # Always delete the temp PEM — even if signing fails
        if ($TempKey -and (Test-Path $TempKey)) { Remove-Item $TempKey -Force }
    }
}
