# Reminds the agent to validate after first-party C++ edits (no auto-build).

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$inputText = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($inputText)) {
    exit 0
}

$payload = $inputText | ConvertFrom-Json
$toolInput = $payload.tool_input

$paths = @()
foreach ($prop in @('path', 'file_path')) {
    if ($toolInput.PSObject.Properties.Name -contains $prop) {
        $value = $toolInput.$prop
        if ($null -ne $value -and "$value" -ne '') {
            $paths += [string]$value
        }
    }
}

function Normalize-PathForMatch {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return '' }
    $normalized = $Path -replace '\\', '/'
    $normalized = $normalized.TrimStart('.', '/')
    if ($normalized -match '^[A-Za-z]:/') {
        $parts = $normalized -split '/'
        $normalized = ($parts | Select-Object -Skip 1) -join '/'
    }
    return $normalized.ToLowerInvariant()
}

function Test-FirstPartyCppPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }

    $normalized = Normalize-PathForMatch $Path
  if ($normalized -notlike 'engine/src/*') { return $false }
    return $normalized -match '\.(cpp|h|hpp|cxx|cc)$'
}

$shouldRemind = $false
foreach ($path in $paths) {
    if (Test-FirstPartyCppPath $path) {
        $shouldRemind = $true
        break
    }
}

if (-not $shouldRemind) {
    exit 0
}

Write-Output (@"
{
  "additional_context": "First-party C++ under engine/src/ was edited. Before finishing, run /validate or: cmake --build build/vs2026-debug --config Debug --target engine_editor, then run engine_editor and exercise the affected path."
}
"@)
exit 0
