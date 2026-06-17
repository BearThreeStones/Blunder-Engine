# Blunder editor viewport profiling — Nsight Systems CLI (2026.x)
# Usage (from repo root):
#   .\profiling\nsight\run-group.ps1 -Group A
#   .\profiling\nsight\run-group.ps1 -Group B -Duration 15

param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('A', 'B', 'C', 'D')]
    [string]$Group = 'A',

    [int]$Duration = 15,

    [ValidateSet('Debug', 'Release')]
    [string]$Config = 'Debug',

    # Resolving PDBs via CLI --resolve-symbols hangs at ~19% with 0-byte .nsys-rep on Windows
    # (verified: rep stays 0 bytes for 60+ min). Use nsys-ui Resolve Symbols after fast capture.
    [switch]$ResolveSymbols
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '_nsight-elevation.ps1')

function Test-IsAdministrator {
    return Test-NsightIsAdministrator
}

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

$groupFiles = @{
    A = 'A_baseline.txt'
    B = 'B_zerocopy.txt'
    C = 'C_no_partial.txt'
    D = 'D_release_baseline.txt'
}

$buildPreset = if ($Config -eq 'Release') { 'vs2026-release' } else { 'vs2026-debug' }
$editorTarget = 'engine_editor'
$buildDir = Join-Path $repoRoot "build\$buildPreset"
$profileExe = Join-Path $repoRoot "build\$buildPreset\engine\src\editor\profile\engine_editor_profile.exe"
$defaultExe = Join-Path $repoRoot "build\$buildPreset\engine\src\editor\$Config\engine_editor.exe"

# Prefer profile output path (avoids locked Debug/engine_editor.exe during relink).
$exe = $profileExe
if (-not (Test-Path $exe)) {
    if (Test-Path $defaultExe) {
        $exe = $defaultExe
        Write-Host "Using existing engine_editor.exe (profile binary not built yet)."
    }
}

$commandFile = Join-Path $PSScriptRoot $groupFiles[$Group]
if (-not (Test-Path $commandFile)) {
    Write-Error "Command file missing: $commandFile"
}

function Find-NsightCli {
    param([string]$Name)

    if ($Name -eq 'nsys' -and $env:BLUNDER_NSIGHT_NSYS -and (Test-Path $env:BLUNDER_NSIGHT_NSYS)) {
        return $env:BLUNDER_NSIGHT_NSYS
    }
    if ($Name -eq 'nsys-ui' -and $env:BLUNDER_NSIGHT_NSYS_UI -and (Test-Path $env:BLUNDER_NSIGHT_NSYS_UI)) {
        return $env:BLUNDER_NSIGHT_NSYS_UI
    }

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $pfRoots = [System.Collections.Generic.HashSet[string]]::new()
    foreach ($value in @($env:ProgramFiles, ${env:ProgramFiles(x86)})) {
        if ($value) { [void]$pfRoots.Add($value) }
    }
    foreach ($letter in (Get-PSDrive -PSProvider FileSystem).Name) {
        [void]$pfRoots.Add("${letter}:\Program Files")
        [void]$pfRoots.Add("${letter}:\Program Files (x86)")
    }

    $found = [System.Collections.Generic.List[string]]::new()
    foreach ($pf in $pfRoots) {
        $nvidiaDir = Join-Path $pf 'NVIDIA Corporation'
        if (-not (Test-Path $nvidiaDir)) { continue }
        Get-ChildItem $nvidiaDir -Filter 'Nsight Systems*' -Directory -ErrorAction SilentlyContinue | ForEach-Object {
            $candidate = Join-Path $_.FullName "target-windows-x64\$Name.exe"
            if (Test-Path $candidate) { $found.Add($candidate) }
        }
    }

    if ($found.Count -eq 0) { return $null }
    return ($found | Sort-Object -Descending | Select-Object -First 1)
}

$nsys = Find-NsightCli -Name 'nsys'
if (-not $nsys) {
    Write-Error @"
nsys not found. Install Nsight Systems 2026.x, or set:
  `$env:BLUNDER_NSIGHT_NSYS = 'D:\Program Files\NVIDIA Corporation\Nsight Systems 2026.3.1\target-windows-x64\nsys.exe'
"@
}

$nsysUi = Find-NsightCli -Name 'nsys-ui'

$reportsDir = Join-Path $PSScriptRoot 'reports'
New-Item -ItemType Directory -Force -Path $reportsDir | Out-Null

function Stop-ProfilingProcesses {
    foreach ($name in @('nsys', 'engine_editor')) {
        Get-Process -Name $name -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host "Stopping leftover $name (PID $($_.Id))..."
            Stop-Process -Id $_.Id -Force -ErrorAction Stop
        }
    }
    if (Get-Process -Name nsys,engine_editor -ErrorAction SilentlyContinue) {
        Start-Sleep -Seconds 2
    }
}

function Remove-ReportArtifacts {
    param([string]$Stem)
    $patterns = @(
        (Join-Path $reportsDir "$Stem.nsys-rep"),
        (Join-Path $reportsDir "$Stem.sqlite"),
        (Join-Path $reportsDir "$Stem.qdrep"),
        (Join-Path $reportsDir "$Stem.qdstrm")
    )
    foreach ($path in $patterns) {
        if (-not (Test-Path $path)) { continue }
        try {
            Remove-Item -LiteralPath $path -Force -ErrorAction Stop
            Write-Host "Removed stale artifact: $path"
        } catch {
            Write-Error "Cannot remove locked artifact: $path`nStop nsys (Ctrl+C / taskkill /F /IM nsys.exe) and re-run."
        }
    }
}

# nsys splits --debug-symbols on ':' — absolute "E:\..." paths break. Use repo-relative paths only.
# Profile exe PDB lives under editor/profile/; static-lib code is folded into the editor PDB.
$symbolDirCandidates = @(
    "build/$buildPreset/engine/src/editor/profile"
    "build/$buildPreset/engine/src/editor/$Config"
    "build/$buildPreset/engine/src/runtime/$Config"
)
$debugSymbolPath = ($symbolDirCandidates -join ':')

Write-Host "Group: $Group | Config: $Config | Duration: ${Duration}s"
Write-Host "Nsys:  $nsys"
Write-Host "Exe:   $exe"
Write-Host "Cfg:   $commandFile"
Write-Host "PDB:   $debugSymbolPath"
Write-Host ""
Write-Host "After launch: idle 2s -> orbit 5s -> pan 3s -> stop."
Write-Host ""

if (-not (Test-IsAdministrator)) {
    $argList = @(
        '-NoProfile', '-ExecutionPolicy', 'Bypass',
        '-File', (Join-Path $PSScriptRoot 'run-group.ps1'),
        '-Group', $Group,
        '-Duration', $Duration,
        '-Config', $Config
    )
    if ($ResolveSymbols) { $argList += '-ResolveSymbols' }
    $symSwitch = if ($ResolveSymbols) { ' -ResolveSymbols' } else { '' }
    $example = ".\profiling\nsight\run-group.ps1 -Group $Group -Duration $Duration -Config $Config$symSwitch"
    if (-not (Invoke-NsightElevation -RepoRoot $repoRoot -ExampleCommand $example -ElevateArgumentList $argList)) {
        exit 2
    }
}

$reportStem = switch ($Group) {
    A { 'blunder_A_debug_baseline' }
    B { 'blunder_B_debug_zerocopy' }
    C { 'blunder_C_debug_no_partial' }
    D { 'blunder_D_release_baseline' }
}
$reportPath = Join-Path $reportsDir "$reportStem.nsys-rep"

# Hung export from a prior run locks .sqlite/.nsys-rep — stop before cleanup.
Stop-ProfilingProcesses

$staleEditor = Get-Process -Name engine_editor -ErrorAction SilentlyContinue
if ($staleEditor) {
    Write-Host "Note: engine_editor.exe still running (PID $($staleEditor.Id -join ',')); using engine_editor_profile.exe for capture."
}

$runtimeLibSuffix = if ($Config -eq 'Debug') { 'd' } else { '' }
$runtimeLib = Join-Path $repoRoot "build\$buildPreset\engine\src\runtime\$Config\engine_runtime$runtimeLibSuffix.lib"
if (-not (Test-Path $runtimeLib)) {
    $runtimeLib = Join-Path $repoRoot "build\$buildPreset\engine\src\runtime\$Config\engine_runtimed.lib"
}
$needsProfileBuild = $false
if ((Test-Path $runtimeLib)) {
    if (-not (Test-Path $profileExe)) {
        $needsProfileBuild = $true
    } elseif ((Test-Path $exe)) {
        $libTime = (Get-Item $runtimeLib).LastWriteTimeUtc
        $exeTime = (Get-Item $exe).LastWriteTimeUtc
        if ($libTime -gt $exeTime) {
            $needsProfileBuild = $true
        }
    }
}
if ($needsProfileBuild) {
    Write-Host "Rebuilding $editorTarget -> profile/engine_editor_profile.exe (runtime lib newer or profile exe missing)..."
    cmake -S $repoRoot -B $buildDir -DBLUNDER_EDITOR_PROFILE_EXE=ON | Out-Null
    cmake --build $buildDir --target $editorTarget --config $Config
    if ($LASTEXITCODE -ne 0) {
        Write-Error "$editorTarget profile rebuild failed. See build output above."
    }
    $exe = $profileExe
}
if (-not (Test-Path $exe)) {
    Write-Error "Editor executable not found: $profileExe (or $defaultExe). Build failed or path wrong."
}

# Stale .sqlite from a prior export + --force-overwrite on .nsys-rep can leave a 0-byte rep and hang export (~19%).
Remove-ReportArtifacts -Stem $reportStem

$commonFile = Join-Path $PSScriptRoot 'common.txt'

$nsysArgs = @(
    'profile',
    "--command-file=$commonFile",
    "--command-file=$commandFile",
    "--duration=$Duration",
    "--debug-symbols=$debugSymbolPath",
    '--force-overwrite=true'
)
if ($ResolveSymbols) {
    Write-Host "ResolveSymbols: post-capture via nsys-ui (CLI --resolve-symbols hangs at 19% on Windows)"
} else {
    Write-Host "ResolveSymbols: OFF (resolve in nsys-ui before SQLite export if you need function names)"
}
$nsysArgs += $exe

$profileStarted = Get-Date
& $nsys @nsysArgs
$profileExit = $LASTEXITCODE
$profileSeconds = [int]((Get-Date) - $profileStarted).TotalSeconds

$reportBytes = if (Test-Path $reportPath) { (Get-Item $reportPath).Length } else { 0 }

if ($profileExit -ne 0) {
    Write-Error "nsys profile failed (exit $profileExit). See messages above."
}
if ($reportBytes -lt 1MB) {
    Write-Error @"
Report looks incomplete ($reportBytes bytes): $reportPath
Kill any hung nsys (taskkill /F /IM nsys.exe), run stop-and-reset.ps1, then re-capture WITHOUT CLI symbol resolve:
  .\profiling\nsight\run-group.ps1 -Group $Group -Duration $Duration
CLI --resolve-symbols hangs at 19%% on Windows. Resolve PDBs in nsys-ui after a valid .nsys-rep exists.
"@
}

if (Test-Path $reportPath) {
    Write-Host ""
    Write-Host "Report: $reportPath"
    $sqlitePath = Join-Path $reportsDir "$reportStem.sqlite"
    if (Test-Path $sqlitePath) {
        $sqliteBytes = (Get-Item $sqlitePath).Length
        Write-Host "SQLite: $sqlitePath ($([math]::Round($sqliteBytes/1MB, 1)) MB)"
    }
    if ($nsysUi) {
        Write-Host "Open:   `"$nsysUi`" `"$reportPath`""
    } else {
        Write-Host "Open:   nsys-ui `"$reportPath`""
    }
    if ($ResolveSymbols) {
        Write-Host ""
        Write-Host "Symbol resolution (required for named CPU stacks in SQLite):"
        Write-Host "  1. Open the .nsys-rep in nsys-ui (command above)"
        Write-Host "  2. Project -> right-click report -> Resolve Symbols..."
        Write-Host "  3. Add PDB folders: build\$buildPreset\engine\src\editor\profile and ...\editor\$Config"
        Write-Host "  4. File -> Export -> SQLite -> $sqlitePath"
        Write-Host "  Or: .\profiling\nsight\export-sqlite.ps1 -Group $Group  (after step 2 completes)"
    }
}
