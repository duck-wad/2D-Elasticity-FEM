# Build fem_engine.exe and fem_modeler.exe into ../dist (flat layout), then remove intermediates.
param(
    [switch]$SkipCpp,
    [switch]$SkipPython
)

$ErrorActionPreference = "Stop"

$SrcRoot = $PSScriptRoot
$RepoRoot = Split-Path $SrcRoot -Parent
$DistDir = Join-Path $RepoRoot "dist"
$BuildDir = Join-Path $RepoRoot "build"
$EngineDir = Join-Path $SrcRoot "Engine"
$ModelerDir = Join-Path $SrcRoot "Modeler"
$EngineSln = Join-Path $EngineDir "Engine.sln"
$EngineOut = Join-Path $EngineDir "bin\fem_engine.exe"
$DistEngine = Join-Path $DistDir "fem_engine.exe"
$DistModeler = Join-Path $DistDir "fem_modeler.exe"
$PyiBundleDir = Join-Path $DistDir "fem_modeler"
$SpecFile = Join-Path $SrcRoot "fem_modeler.spec"

function Write-Step([string]$Message) {
    $ts = Get-Date -Format "HH:mm:ss"
    Write-Host "[$ts] $Message"
    [Console]::Out.Flush()
}

function Remove-Tree([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return }
    Write-Step "Removing $Path"
    try {
        Remove-Item -LiteralPath $Path -Recurse -Force -ErrorAction Stop
    }
    catch {
        throw @"
Cannot delete '$Path' (file may be in use).
Close dist\fem_modeler.exe and any Explorer windows open in dist, then run build again.
"@
    }
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
        if ($found) {
            return [string]$found
        }
    }

    throw "MSBuild not found. Install Visual Studio 2022 with C++ desktop development."
}

function Get-PythonExe {
    $exe = (Get-Command python -ErrorAction SilentlyContinue).Source
    if (-not $exe) {
        throw "python not found on PATH."
    }
    return $exe
}

function Test-PythonImport([string]$ImportName, [string]$PythonExe) {
    $prevEap = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        & $PythonExe -c "import $ImportName" 2>$null | Out-Null
        return ($LASTEXITCODE -eq 0)
    }
    finally {
        $ErrorActionPreference = $prevEap
    }
}

function Ensure-PythonTool([string]$ImportName, [string]$PythonExe, [string]$PipName) {
    if (-not $PipName) { $PipName = $ImportName }
    Write-Step "Checking $ImportName..."
    if (Test-PythonImport $ImportName $PythonExe) {
        Write-Step "  $ImportName OK"
        return
    }
    Write-Step "Installing $PipName..."
    & $PythonExe -m pip install $PipName
    if ($LASTEXITCODE -ne 0) { throw "pip install $PipName failed." }
    if (-not (Test-PythonImport $ImportName $PythonExe)) {
        throw "Installed $PipName but cannot import $ImportName."
    }
}

function Move-MergeIntoDist([string]$SourceDir, [string]$TargetDir) {
    Get-ChildItem -LiteralPath $SourceDir -Force | ForEach-Object {
        $dest = Join-Path $TargetDir $_.Name
        if ($_.PSIsContainer) {
            if (-not (Test-Path -LiteralPath $dest)) {
                New-Item -ItemType Directory -Path $dest -Force | Out-Null
            }
            Move-MergeIntoDist $_.FullName $dest
            Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
        }
        elseif (Test-Path -LiteralPath $dest) {
            Remove-Item -LiteralPath $dest -Force
            Move-Item -LiteralPath $_.FullName -Destination $TargetDir -Force
        }
        else {
            Move-Item -LiteralPath $_.FullName -Destination $TargetDir -Force
        }
    }
}

Write-Host ""
Write-Step "Tip: close dist\fem_modeler.exe before building (avoids locked files)."
Write-Host ""

if (-not $SkipPython) {
    Write-Step "Preparing dist folder"
    Remove-Tree $DistDir
    Remove-Tree $BuildDir
    New-Item -ItemType Directory -Path $DistDir -Force | Out-Null
}
elseif (-not $SkipCpp) {
    Remove-Tree $BuildDir
}

if (-not $SkipCpp) {
    Write-Step "Building C++ engine (Release x64)"
    $msbuildExe = Get-MSBuildPath
    Write-Step "  MSBuild: $msbuildExe"
    # Invoke in-process so the script continues as soon as MSBuild truly exits.
    & $msbuildExe $EngineSln /p:Configuration=Release /p:Platform=x64 /v:minimal /m
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    Write-Step "MSBuild finished"
    if (-not (Test-Path -LiteralPath $EngineOut)) {
        throw "Build succeeded but executable not found at $EngineOut"
    }
}
elseif (-not (Test-Path -LiteralPath $EngineOut)) {
    throw "SkipCpp set but $EngineOut is missing. Run a full build first."
}

if (-not $SkipPython) {
    Write-Step "Building Python modeler (PyInstaller) - expect 2-6 minutes"
    Write-Step "  Long pause on hook-PySide6.QtGui is normal."

    $pythonExe = Get-PythonExe
    Write-Step "  Python: $pythonExe"

    Ensure-PythonTool -ImportName "PyInstaller" -PythonExe $pythonExe -PipName "pyinstaller"
    Ensure-PythonTool -ImportName "PySide6" -PythonExe $pythonExe -PipName "PySide6"

    $pyiWork = Join-Path $BuildDir "pyinstaller-work"
    New-Item -ItemType Directory -Path $pyiWork -Force | Out-Null

    $env:PYTHONUNBUFFERED = "1"
    Push-Location $SrcRoot
    try {
        Write-Step "Starting PyInstaller..."
        & $pythonExe -m PyInstaller `
            --noconfirm `
            --clean `
            --log-level INFO `
            --distpath $DistDir `
            --workpath $pyiWork `
            $SpecFile
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Pop-Location
    }

    if (-not (Test-Path -LiteralPath $PyiBundleDir)) {
        throw "PyInstaller finished but bundle folder not found at $PyiBundleDir"
    }

    Write-Step "Flattening dist layout"
    Move-MergeIntoDist $PyiBundleDir $DistDir
    Remove-Tree $PyiBundleDir
}

if (-not $SkipCpp) {
    Copy-Item -LiteralPath $EngineOut -Destination $DistEngine -Force
}
elseif (-not (Test-Path -LiteralPath $DistEngine) -and (Test-Path -LiteralPath $EngineOut)) {
    Copy-Item -LiteralPath $EngineOut -Destination $DistEngine -Force
}

if (-not (Test-Path -LiteralPath $DistModeler)) {
    throw "Expected modeler executable at $DistModeler"
}
if (-not (Test-Path -LiteralPath $DistEngine)) {
    throw "Expected engine executable at $DistEngine"
}

Write-Step "C++ engine   -> $DistEngine"
Write-Step "Python modeler -> $DistModeler"

Write-Step "Cleaning intermediate build artifacts"
Remove-Tree (Join-Path $EngineDir "bin")
Remove-Tree (Join-Path $EngineDir "obj")
Remove-Tree $BuildDir
Remove-Tree (Join-Path $ModelerDir "build")
Remove-Tree (Join-Path $ModelerDir "dist")
Remove-Tree (Join-Path $ModelerDir "__pycache__")
Get-ChildItem -Path $ModelerDir -Filter "*.spec" -File -ErrorAction SilentlyContinue |
    Remove-Item -Force

Write-Host ""
Write-Step "Done. Run the modeler:"
Write-Host "  $DistModeler"
Write-Host ""
Write-Host 'Distribution (flat dist folder):'
Write-Host '  dist\fem_engine.exe'
Write-Host '  dist\fem_modeler.exe'
Write-Host '  dist\_internal\   (Qt/Python runtime for the modeler)'
Write-Host ""
Write-Host 'Faster rebuilds:'
Write-Host '  .\src\build.ps1 -SkipCpp      # Python only'
Write-Host '  .\src\build.ps1 -SkipPython   # C++ only'
