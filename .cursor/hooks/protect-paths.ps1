# Blocks writes to vendor, build, and cache directories.
# Whitelist: engine/3rdparty/slint/**
# Read one JSON object from stdin (do not wait for EOF — Cursor may leave the pipe open).

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Read-HookStdinObjectText {
    $reader = [Console]::In
    $sb = New-Object System.Text.StringBuilder
    $depth = 0
    $inString = $false
    $escape = $false
    $started = $false
    while ($true) {
        $code = $reader.Read()
        if ($code -lt 0) { break }
        $ch = [char]$code
        [void]$sb.Append($ch)
        if ($inString) {
            if ($escape) { $escape = $false; continue }
            if ($ch -eq [char]92) { $escape = $true; continue }
            if ($ch -eq [char]34) { $inString = $false }
            continue
        }
        if ($ch -eq [char]34) { $inString = $true; continue }
        if ($ch -eq [char]123) { $depth++; $started = $true; continue }
        if ($ch -eq [char]125) {
            $depth--
            if ($started -and $depth -eq 0) { break }
        }
    }
    return $sb.ToString()
}

$inputText = Read-HookStdinObjectText
if ([string]::IsNullOrWhiteSpace($inputText)) {
    Write-Output '{ "permission": "allow" }'
    exit 0
}

$payload = $inputText | ConvertFrom-Json
$toolInput = $null
if ($null -ne $payload -and $payload.PSObject.Properties.Name -contains 'tool_input') {
    $toolInput = $payload.tool_input
}

$paths = @()
if ($null -ne $toolInput) {
    foreach ($prop in @('path', 'file_path')) {
        if ($toolInput.PSObject.Properties.Name -contains $prop) {
            $value = $toolInput.$prop
            if ($null -ne $value -and "$value" -ne '') {
                $paths += [string]$value
            }
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
