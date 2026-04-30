#!/usr/bin/env pwsh
# setup_crypto.ps1 -- One-time setup: vendor Monocypher 3.1.3 into shared/crypto/vendor/
#
# Run from the repository root (or from shared/crypto/):
#   powershell -ExecutionPolicy Bypass -File shared\crypto\setup_crypto.ps1
#
# What it does:
#   1. Downloads monocypher-3.1.3.tar.gz from GitHub via Windows HTTPS
#   2. Extracts only the files needed for Ed25519 verification
#   3. Places them in shared/crypto/vendor/monocypher-3.1.3/
#
# After running this once, all subsequent builds work fully offline.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ScriptDir  = $PSScriptRoot
if (-not $ScriptDir) { $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path }

$VendorDir  = Join-Path $ScriptDir "vendor\monocypher-3.1.3"
$TarballUrl = "https://github.com/LoupVaillant/Monocypher/archive/refs/tags/3.1.3.tar.gz"
$TmpTar     = Join-Path ([System.IO.Path]::GetTempPath()) "monocypher-3.1.3.tar.gz"
$TmpExtract = Join-Path ([System.IO.Path]::GetTempPath()) "monocypher-extract"

# Already vendored?
$CoreFile = Join-Path $VendorDir "src\monocypher.c"
if (Test-Path $CoreFile) {
    Write-Host "Monocypher already vendored at $VendorDir -- nothing to do."
    exit 0
}

# Download
Write-Host "Downloading Monocypher 3.1.3 from GitHub..."
Invoke-WebRequest -Uri $TarballUrl -OutFile $TmpTar -UseBasicParsing
Write-Host "  -> $TmpTar"

# Extract via tar (bundled with Windows 10+)
if (Test-Path $TmpExtract) { Remove-Item $TmpExtract -Recurse -Force }
New-Item -ItemType Directory $TmpExtract | Out-Null
tar -xzf $TmpTar -C $TmpExtract
$ExtractedDir = Get-ChildItem $TmpExtract -Directory | Select-Object -First 1

# Copy required sources
$SrcDir = Join-Path $ExtractedDir.FullName "src"

New-Item -ItemType Directory -Force (Join-Path $VendorDir "src")          | Out-Null
New-Item -ItemType Directory -Force (Join-Path $VendorDir "src\optional") | Out-Null

Copy-Item (Join-Path $SrcDir "monocypher.c")                   (Join-Path $VendorDir "src\")
Copy-Item (Join-Path $SrcDir "monocypher.h")                   (Join-Path $VendorDir "src\")
Copy-Item (Join-Path $SrcDir "optional\monocypher-ed25519.c")  (Join-Path $VendorDir "src\optional\")
Copy-Item (Join-Path $SrcDir "optional\monocypher-ed25519.h")  (Join-Path $VendorDir "src\optional\")

# Copy license for attribution
$LicenseFile = Join-Path $ExtractedDir.FullName "LICENCE.md"
if (Test-Path $LicenseFile) { Copy-Item $LicenseFile (Join-Path $VendorDir "\") }

# Cleanup
Remove-Item $TmpTar     -Force
Remove-Item $TmpExtract -Recurse -Force

Write-Host ""
Write-Host "Monocypher 3.1.3 vendored to $VendorDir"
Write-Host "You can now configure and build:"
Write-Host "  cmake -S . -B build --toolchain cmake/arm-none-eabi.cmake -G 'Ninja Multi-Config'"
Write-Host "  cmake --build build --config Debug"
