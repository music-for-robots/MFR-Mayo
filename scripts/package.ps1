# MFR-Mayo - Copyright (C) 2026 Music For Robots
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Packages a Release build into dist/: a zip and an Inno Setup installer.
# Usage: scripts/package.ps1 [-BuildDir build] [-Iscc path\to\ISCC.exe]
param(
    [string]$BuildDir = "build",
    [string]$Iscc = ""
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot

# Single source of truth for the version: project(... VERSION x.y.z) in CMakeLists.txt
$cmake = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cmake -notmatch 'project\(\w+ VERSION (\d+\.\d+\.\d+)') { throw "Couldn't find the project version in CMakeLists.txt" }
$version = $Matches[1]

$release = Join-Path $root "$BuildDir\Mayo_artefacts\Release"
$vst3 = Join-Path $release "VST3\MFR-Mayo.vst3"
$exe = Join-Path $release "Standalone\MFR-Mayo.exe"
foreach ($p in $vst3, $exe) { if (-not (Test-Path $p)) { throw "Missing build output: $p" } }

$dist = Join-Path $root "dist"
$stage = Join-Path $dist "MFR-Mayo-$version-win64"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null

Copy-Item -Recurse $vst3 $stage
Copy-Item $exe $stage
Copy-Item (Join-Path $root "installer\README.txt") $stage
Copy-Item (Join-Path $root "LICENSE") (Join-Path $stage "LICENSE.txt")

$zip = Join-Path $dist "MFR-Mayo-$version-win64.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
# Add entries by hand so paths use "/" (Windows PowerShell's zip helpers write "\",
# which macOS and some unzip tools mishandle).
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    Get-ChildItem $stage -Recurse -File | ForEach-Object {
        $name = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $_.FullName, $name, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally { $archive.Dispose() }
Remove-Item -Recurse -Force $stage

if (-not $Iscc) {
    $Iscc = @(
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
        "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Iscc) { throw "Inno Setup (ISCC.exe) not found" }

& $Iscc /Q "/DAppVersion=$version" "/DBuildDir=$release" "/DOutDir=$dist" (Join-Path $root "installer\MFR-Mayo.iss")
if ($LASTEXITCODE -ne 0) { throw "ISCC failed with exit code $LASTEXITCODE" }

Get-ChildItem $dist -File | ForEach-Object { "{0}  {1:N1} MB" -f $_.Name, ($_.Length / 1MB) }
