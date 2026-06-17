# Blocks writes to vendor, build, and cache directories.
# Whitelist: engine/3rdparty/slint/**

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$inputText = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($inputText)) {
    Write-Output '{ "permission": "allow" }'
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

function Test-BlockedPath {
    param([string]$Path)

    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }

    $normalized = Normalize-PathForMatch $Path

    if ($normalized -like 'engine/3rdparty/slint/*' -or $normalized -eq 'engine/3rdparty/slint') {
        return $false
    }
    if ($normalized -like 'engine/3rdparty/*' -or $normalized -eq 'engine/3rdparty') {
        return $true
    }
    if ($normalized -like 'build/*' -or $normalized -eq 'build') {
        return $true
    }
    if ($normalized -like '.blunder/*' -or $normalized -eq '.blunder') {
        return $true
    }
    if ($normalized -like '.cmake_deps/*' -or $normalized -eq '.cmake_deps') {
        return $true
    }

    return $false
}

foreach ($path in $paths) {
    if (Test-BlockedPath $path) {
        $escaped = $path -replace '"', '\"'
        Write-Output (@"
{
  "permission": "deny",
  "user_message": "Blocked edit to protected path: $escaped",
  "agent_message": "Do not edit vendor/build/cache paths. Allowed exception: engine/3rdparty/slint/. Edit first-party code under engine/src/ instead."
}
"@)
        exit 0
    }
}

Write-Output '{ "permission": "allow" }'
exit 0
