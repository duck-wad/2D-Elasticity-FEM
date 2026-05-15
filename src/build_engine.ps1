# Build fem_engine.exe only (Release x64). Fast path for C++ development.
$ErrorActionPreference = "Stop"

$SrcRoot = $PSScriptRoot
$RepoRoot = Split-Path $SrcRoot -Parent
$EngineDir = Join-Path $SrcRoot "Engine"
$EngineSln = Join-Path $EngineDir "Engine.sln"
$EngineOut = Join-Path $EngineDir "bin\fem_engine.exe"
$DistEngine = Join-Path $RepoRoot "dist\fem_engine.exe"

function Write-Step([string]$Message) {
    $ts = Get-Date -Format "HH:mm:ss"
    Write-Host "[$ts] $Message"
}

function Get-MSBuildPath {
    $candidates = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe"
    )
    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $found = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" 2>$null |
            Select-Object -First 1
        if ($found) { return [string]$found }
    }

    throw "MSBuild not found. Install Visual Studio 2022 with C++ desktop development."
}

Write-Step "Building C++ engine (Release x64)"
$msbuildExe = Get-MSBuildPath
& $msbuildExe $EngineSln /p:Configuration=Release /p:Platform=x64 /v:minimal /m
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not (Test-Path -LiteralPath $EngineOut)) {
    throw "Build finished but executable not found at $EngineOut"
}

Write-Step "Built -> $EngineOut"

if (Test-Path -LiteralPath (Split-Path $DistEngine -Parent)) {
    Copy-Item -LiteralPath $EngineOut -Destination $DistEngine -Force
    Write-Step "Updated dist -> $DistEngine"
}

Write-Step "Done."
