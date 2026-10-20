<#
.SYNOPSIS
    Builds CycloneDDS (C) and CycloneDDS-CXX in BOTH Debug and Release and installs them into ONE prefix.

.DESCRIPTION
    Debug libraries get a "d" postfix (ddscd.dll, ddscxxd.dll), so a single install serves Debug and Release
    builds of this project without clashing. Run it once per machine, from any PowerShell window
    (no admin rights needed unless -Prefix points somewhere protected).

    Build Cyclone with the OLDEST Visual Studio you will use for this project (e.g. VS 2022 if you use both
    VS 2022 and VS 2026): binaries built by an older toolset can be consumed by a newer one, not the other way round.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\setup_cyclonedds.ps1
.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\setup_cyclonedds.ps1 -Prefix D:\sdk\cyclonedds-11.0.1 -Generator "Visual Studio 17 2022"
#>
param(
    # CycloneDDS / CycloneDDS-CXX git tag. Must match what the generated Unitree types were made with.
    [string]$Version = "11.0.1",
    # Install prefix. Point the CYCLONEDDS_ROOT environment variable (or CMake cache variable) here.
    [string]$Prefix = $(if ($env:CYCLONEDDS_ROOT) { $env:CYCLONEDDS_ROOT } else { Join-Path $env:USERPROFILE "cyclonedds-11.0.1" }),
    # Where sources are cloned and built.
    [string]$WorkDir = $(Join-Path $env:USERPROFILE "cyclonedds-build"),
    # CMake generator. Empty = CMake picks the newest installed Visual Studio.
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param([string]$Exe, [string[]]$Arguments)
    Write-Host ">> $Exe $($Arguments -join ' ')" -ForegroundColor Cyan
    & $Exe @Arguments
    if ($LASTEXITCODE -ne 0) { throw "'$Exe' failed with exit code $LASTEXITCODE" }
}

$genArgs = @()
if ($Generator) { $genArgs = @("-G", $Generator) }

# Deterministic, dependency-free Cyclone: no OpenSSL/DDS-Security (not used by the Unitree Go2), no LTO.
$common = @(
    "-A", "x64",
    "-DCMAKE_INSTALL_PREFIX=$Prefix",
    "-DCMAKE_PREFIX_PATH=$Prefix",
    "-DCMAKE_DEBUG_POSTFIX=d",
    "-DBUILD_EXAMPLES=OFF",
    "-DBUILD_TESTING=OFF",
    "-DENABLE_LTO=OFF"
)

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

foreach ($repo in @("cyclonedds", "cyclonedds-cxx")) {
    $src = Join-Path $WorkDir "$repo-$Version"
    $bld = Join-Path $WorkDir "$repo-$Version-build"

    if (-not (Test-Path $src)) {
        Invoke-Checked git @("clone", "--depth", "1", "--branch", $Version, "https://github.com/eclipse-cyclonedds/$repo.git", $src)
    }

    $extra = @()
    if ($repo -eq "cyclonedds") {
        $extra = @("-DENABLE_SSL=OFF", "-DENABLE_SECURITY=OFF", "-DBUILD_DDSPERF=OFF")
    }

    Invoke-Checked cmake (@("-S", $src, "-B", $bld) + $genArgs + $common + $extra)

    # Release last, so the single (un-postfixed) idlc.exe left in <prefix>/bin is the Release one.
    foreach ($cfg in @("Debug", "Release")) {
        Invoke-Checked cmake @("--build", $bld, "--config", $cfg, "--target", "install", "--parallel")
    }
}

Write-Host ""
Write-Host "CycloneDDS $Version (Debug + Release) installed to: $Prefix" -ForegroundColor Green
if ($env:CYCLONEDDS_ROOT -ne $Prefix) {
    Write-Host "Tell the project where it is, e.g. (then restart Visual Studio):" -ForegroundColor Yellow
    Write-Host "    setx CYCLONEDDS_ROOT `"$Prefix`""
}
