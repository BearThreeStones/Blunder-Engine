# Export .nsys-rep to SQLite (run AFTER resolving symbols in nsys-ui for named stacks).
# Usage: .\profiling\nsight\export-sqlite.ps1 -Group A

param(
    [Parameter(Mandatory = $false)]
    [ValidateSet('A', 'B', 'C', 'D')]
    [string]$Group = 'A'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '..\..')
Set-Location $repoRoot

$stems = @{
    A = 'blunder_A_debug_baseline'
    B = 'blunder_B_debug_zerocopy'
    C = 'blunder_C_debug_no_partial'
    D = 'blunder_D_release_baseline'
}
$stem = $stems[$Group]
$rep = Join-Path $PSScriptRoot "reports\$stem.nsys-rep"
$sqlite = Join-Path $PSScriptRoot "reports\$stem.sqlite"

if (-not (Test-Path $rep)) {
    Write-Error "Report not found: $rep`nRun .\profiling\nsight\run-group.ps1 -Group $Group first."
}
$repBytes = (Get-Item $rep).Length
if ($repBytes -lt 1MB) {
    Write-Error "Report incomplete ($repBytes bytes). Re-capture without CLI -ResolveSymbols (it hangs at 19%)."
}

$nsys = $env:BLUNDER_NSIGHT_NSYS
if (-not $nsys -or -not (Test-Path $nsys)) {
    $cmd = Get-Command nsys -ErrorAction SilentlyContinue
    if ($cmd) { $nsys = $cmd.Source }
}
if (-not $nsys) {
    Write-Error "nsys not found. Set `$env:BLUNDER_NSIGHT_NSYS."
}

Write-Host "Exporting $rep -> $sqlite"
& $nsys export --type=sqlite --output=$sqlite --force-overwrite=true $rep
if ($LASTEXITCODE -ne 0) {
    Write-Error "nsys export failed (exit $LASTEXITCODE)"
}
Write-Host "Done: $sqlite ($([math]::Round((Get-Item $sqlite).Length/1MB, 1)) MB)"
