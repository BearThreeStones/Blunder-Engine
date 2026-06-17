# Kill hung nsys / engine_editor and remove stale report artifacts for one group.
# Run from repo root (elevates via UAC if needed):
#   .\profiling\nsight\stop-and-reset.ps1 -Group A

param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('A', 'B', 'C', 'D')]
    [string]$Group = 'A'
)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot '_nsight-elevation.ps1')

$stems = @{
    A = 'blunder_A_debug_baseline'
    B = 'blunder_B_debug_zerocopy'
    C = 'blunder_C_debug_no_partial'
    D = 'blunder_D_release_baseline'
}
$stem = $stems[$Group]
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
$reportsDir = Join-Path $PSScriptRoot 'reports'

$inner = @"
Set-Location '$repoRoot'
Write-Host 'Stopping nsys / engine_editor...'
Get-Process -Name nsys,engine_editor -ErrorAction SilentlyContinue | ForEach-Object {
    Write-Host "  kill PID `$(`$_.Id) `$(`$_.ProcessName)"
    Stop-Process -Id `$_.Id -Force
}
Start-Sleep -Seconds 2
`$exts = @('.nsys-rep', '.sqlite', '.qdrep', '.qdstrm')
foreach (`$ext in `$exts) {
    `$path = Join-Path '$reportsDir' ('$stem' + `$ext)
    if (Test-Path `$path) {
        Remove-Item -LiteralPath `$path -Force
        Write-Host "Removed `$path"
    }
}
Write-Host ''
Write-Host 'Done. Next:'
Write-Host "  .\profiling\nsight\run-group.ps1 -Group $Group -Duration 15 -ResolveSymbols"
"@

if (-not (Test-NsightIsAdministrator)) {
    $tempScript = Join-Path $env:TEMP "blunder-nsight-reset-$Group.ps1"
    Set-Content -Path $tempScript -Value $inner -Encoding UTF8
    $example = ".\profiling\nsight\stop-and-reset.ps1 -Group $Group"
    $argList = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $tempScript)
    if (-not (Invoke-NsightElevation -RepoRoot $repoRoot -ExampleCommand $example -ElevateArgumentList $argList)) {
        exit 2
    }
}

Invoke-Expression $inner
