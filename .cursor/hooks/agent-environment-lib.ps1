# Shared Agent environment helpers for path stems, Test runs, and Completion evidence.
# Dot-source from hook scripts. Compatible with Windows PowerShell 5.1.

Set-StrictMode -Version Latest

$script:GenericStems = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
foreach ($stem in @(
        'system', 'test', 'tests', 'src', 'runtime', 'function', 'editor', 'core',
        'impl', 'util', 'utils', 'common', 'base', 'internal', 'controller', 'helper',
        'cpp', 'h', 'hpp', 'cxx', 'cc'
    )) {
    [void]$script:GenericStems.Add($stem)
}

$script:PromotionArmingMarker = 'BLUNDER_PROMOTION_ARMING'

function ConvertTo-ObjectList {
    param($Value)
    $list = New-Object System.Collections.Generic.List[object]
    if ($null -eq $Value) { return , $list }
    if ($Value -is [string]) {
        $list.Add($Value)
        return , $list
    }
    if ($Value -is [System.Array] -or $Value -is [System.Collections.IList]) {
        foreach ($item in $Value) {
            $list.Add($item)
        }
        return , $list
    }
    $list.Add($Value)
    return , $list
}

function ConvertTo-ObjectArray {
    param($Value)
    if ($null -eq $Value) { return @() }
    if ($Value -is [System.Array]) { return @($Value) }
    return @($Value)
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

function ConvertTo-RepoRelativePath {
    param(
        [string]$Path,
        [string]$RepoRoot
    )
    if ([string]::IsNullOrWhiteSpace($Path)) { return '' }
    if ([string]::IsNullOrWhiteSpace($RepoRoot)) {
        return ($Path -replace '\\', '/')
    }
    try {
        $full = [IO.Path]::GetFullPath($Path)
        $root = [IO.Path]::GetFullPath($RepoRoot).TrimEnd('\', '/')
        if ($full.StartsWith($root, [StringComparison]::OrdinalIgnoreCase)) {
            $rel = $full.Substring($root.Length).TrimStart('\', '/')
            return ($rel -replace '\\', '/')
        }
    } catch {
    }
    return ($Path -replace '\\', '/')
}

function Test-IsFirstPartyCppPath {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    $normalized = Normalize-PathForMatch $Path
    if ($normalized -notmatch '(^|/)engine/src/') { return $false }
    return $normalized -match '\.(cpp|h|hpp|cxx|cc)$'
}

function Get-PathStems {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path)) { return @() }
    $name = [IO.Path]::GetFileNameWithoutExtension($Path)
    if ([string]::IsNullOrWhiteSpace($name)) { return @() }
    $parts = @($name -split '[_\-]+' | Where-Object { $_ -ne '' })
    $stems = New-Object System.Collections.Generic.List[string]
    foreach ($part in $parts) {
        if (-not $script:GenericStems.Contains($part)) {
            $stems.Add($part.ToLowerInvariant())
        }
    }
    return , $stems
}

function Test-IsTestRun {
    param([string]$Command)
    if ([string]::IsNullOrWhiteSpace($Command)) { return $false }
    if ($Command -match '(?i)(^|[\\/\s''"])ctest(\.exe)?(\s|$)') { return $true }
    if ($Command -match '(?i)[\w.-]+_tests?\.exe') { return $true }
    $withoutTarget = $Command -replace '(?i)--target(\s+|=)[\w.-]+', ' '
    if ($withoutTarget -match '(?i)(^|[\\/\s''"])(\.\/)?[\w.-]+_tests?(\s|$)') { return $true }
    return $false
}

function Test-IsEditorBuild {
    param([string]$Command)
    if ([string]::IsNullOrWhiteSpace($Command)) { return $false }
    if ($Command -notmatch '(?i)(^|[\\/\s''"])cmake(\.exe)?(\s|$)') { return $false }
    if ($Command -notmatch '(?i)--build') { return $false }
    return $Command -match '(?i)engine_editor'
}

function Get-CommandKind {
    param([string]$Command)
    if (Test-IsTestRun $Command) { return 'test_run' }
    if (Test-IsEditorBuild $Command) { return 'editor_build' }
    return $null
}

function Test-CommandContainsAnyStem {
    param(
        [string]$Command,
        [string[]]$Stems
    )
    if ([string]::IsNullOrWhiteSpace($Command)) { return $false }
    if ($null -eq $Stems -or @($Stems).Count -eq 0) { return $false }
    $lower = $Command.ToLowerInvariant()
    foreach ($stem in @($Stems)) {
        if ([string]::IsNullOrWhiteSpace($stem)) { continue }
        if ($lower.Contains([string]$stem.ToLowerInvariant())) { return $true }
    }
    return $false
}

function Test-StemMatchesTestName {
    param(
        [string]$Stem,
        [string[]]$TestNames
    )
    if ([string]::IsNullOrWhiteSpace($Stem)) { return $false }
    $needle = $Stem.ToLowerInvariant()
    foreach ($name in @($TestNames)) {
        if ([string]::IsNullOrWhiteSpace($name)) { continue }
        if ($name.ToLowerInvariant().Contains($needle)) { return $true }
    }
    return $false
}

function Test-EditNeedsTestRun {
    param(
        [string[]]$Stems,
        [string[]]$TestNames
    )
    foreach ($stem in @($Stems)) {
        if (Test-StemMatchesTestName $stem $TestNames) { return $true }
    }
    return $false
}

function Get-PayloadProperty {
    param(
        $Object,
        [string]$Name
    )
    if ($null -eq $Object) { return $null }
    if ($Object.PSObject.Properties.Name -contains $Name) {
        return $Object.$Name
    }
    return $null
}

function Get-ToolOutputObject {
    param($Payload)
    $raw = Get-PayloadProperty $Payload 'tool_output'
    if ($null -eq $raw) { return $null }
    if ($raw -is [string]) {
        if ([string]::IsNullOrWhiteSpace($raw)) { return $null }
        try {
            return $raw | ConvertFrom-Json
        } catch {
            return $null
        }
    }
    return $raw
}

function Get-PayloadExitCode {
    param($Payload)
    foreach ($name in @('exit_code', 'exitCode')) {
        $value = Get-PayloadProperty $Payload $name
        if ($null -ne $value -and "$value" -ne '') {
            try { return [int]$value } catch { }
        }
    }
    $toolOutput = Get-ToolOutputObject $Payload
    if ($null -ne $toolOutput) {
        foreach ($name in @('exitCode', 'exit_code')) {
            $value = Get-PayloadProperty $toolOutput $name
            if ($null -ne $value -and "$value" -ne '') {
                try { return [int]$value } catch { }
            }
        }
    }
    return $null
}

function Get-PayloadCommand {
    param($Payload)
    $direct = Get-PayloadProperty $Payload 'command'
    if ($null -ne $direct -and "$direct" -ne '') { return [string]$direct }
    $toolInput = Get-PayloadProperty $Payload 'tool_input'
    if ($null -ne $toolInput) {
        $fromInput = Get-PayloadProperty $toolInput 'command'
        if ($null -ne $fromInput -and "$fromInput" -ne '') { return [string]$fromInput }
    }
    return ''
}

function Get-PayloadOutput {
    param($Payload)
    $direct = Get-PayloadProperty $Payload 'output'
    if ($null -ne $direct) { return [string]$direct }
    $toolOutput = Get-ToolOutputObject $Payload
    if ($null -ne $toolOutput) {
        foreach ($name in @('stdout', 'output')) {
            $value = Get-PayloadProperty $toolOutput $name
            if ($null -ne $value) { return [string]$value }
        }
    }
    $raw = Get-PayloadProperty $Payload 'tool_output'
    if ($raw -is [string]) { return [string]$raw }
    return ''
}

function Get-PayloadFilePaths {
    param($Payload)
    $paths = @()
    $filePath = Get-PayloadProperty $Payload 'file_path'
    if ($null -ne $filePath -and "$filePath" -ne '') {
        $paths += [string]$filePath
    }
    $toolInput = Get-PayloadProperty $Payload 'tool_input'
    if ($null -ne $toolInput) {
        foreach ($prop in @('path', 'file_path')) {
            $value = Get-PayloadProperty $toolInput $prop
            if ($null -ne $value -and "$value" -ne '') {
                $paths += [string]$value
            }
        }
    }
    return @($paths | Select-Object -Unique)
}

function Test-ObservedSuccess {
    param(
        $Payload,
        [string]$Command,
        [string]$Output
    )
    $code = Get-PayloadExitCode $Payload
    if ($null -ne $code) { return $code -eq 0 }

    $out = [string]$Output
    if ([string]::IsNullOrWhiteSpace($out)) { return $false }

    $kind = Get-CommandKind $Command
    if ($kind -eq 'test_run') {
        if ($out -match '(?i)The following tests FAILED') { return $false }
        if ($out -match '(?i)No tests were found') { return $false }
        if ($out -match '(?i)#+\s*FAIL') { return $false }
        if ($out -match '(?i)100% tests passed') { return $true }
        if ($out -match '(?i)(?<!\d)0% tests passed') { return $false }
        return $false
    }
    if ($kind -eq 'editor_build') {
        if ($out -match '(?i)Build FAILED') { return $false }
        if ($out -match '(?i)ninja: error') { return $false }
        if ($out -match '(?i)Build succeeded') { return $true }
        if ($out -match '(?i)Built target\s+engine_editor') { return $true }
        if ($out -match '(?i)ninja: no work to do') { return $true }
        return $false
    }
    return $false
}

function Test-ObservedFailure {
    param(
        $Payload,
        [string]$Command,
        [string]$Output
    )
    if (-not (Test-IsTestRun $Command)) { return $false }
    $out = [string]$Output
    if ($out -match '(?i)No tests were found') { return $false }
    $code = Get-PayloadExitCode $Payload
    if ($null -ne $code) { return $code -ne 0 }
    if ($out -match '(?i)The following tests FAILED') { return $true }
    if ($out -match '(?i)(?<!\d)0% tests passed') { return $true }
    if ($out -match '(?i)#+\s*FAIL') { return $true }
    if ($out -match '(?i)100% tests passed') { return $false }
    return $false
}

function ConvertTo-TestIdentity {
    param([string]$Name)
    if ([string]::IsNullOrWhiteSpace($Name)) { return '' }
    $s = $Name.Trim().ToLowerInvariant()
    $s = $s.Trim('"', "'")
    $s = $s -replace '\.exe$', ''
    if ($s.EndsWith('_tests') -and $s.Length -gt 6) {
        $s = $s.Substring(0, $s.Length - 6)
    } elseif ($s.EndsWith('_test') -and $s.Length -gt 5) {
        $s = $s.Substring(0, $s.Length - 5)
    }
    return $s
}

function Get-TestIdentityKeys {
    param([string]$Command)
    $keys = New-Object System.Collections.Generic.List[string]
    if ([string]::IsNullOrWhiteSpace($Command)) { return , $keys }
    $withoutTarget = $Command -replace '(?i)--target(\s+|=)[\w.-]+', ' '
    $exeMatches = [regex]::Matches($withoutTarget, '(?i)[\w.-]+_tests?(?:\.exe)?')
    foreach ($match in $exeMatches) {
        $id = ConvertTo-TestIdentity $match.Value
        if ($id -ne '' -and -not $keys.Contains($id)) { $keys.Add($id) }
    }
    $regexMatches = [regex]::Matches($Command, '(?i)(?:-R|--tests-regex)(?:\s+|=)[''"]?([^''"\s]+)')
    foreach ($match in $regexMatches) {
        $raw = [string]$match.Groups[1].Value
        foreach ($part in ($raw -split '\|')) {
            $id = ConvertTo-TestIdentity $part
            if ($id -ne '' -and -not $keys.Contains($id)) { $keys.Add($id) }
        }
    }
    return , $keys
}

function Test-TestIdentitiesOverlap {
    param(
        $Left,
        $Right
    )
    foreach ($x in (ConvertTo-ObjectList $Left)) {
        if ([string]::IsNullOrWhiteSpace([string]$x)) { continue }
        foreach ($y in (ConvertTo-ObjectList $Right)) {
            if ([string]$x -eq [string]$y) { return $true }
        }
    }
    return $false
}

function Test-IsFirstPartyTestPath {
    param([string]$Path)
    if (-not (Test-IsFirstPartyCppPath $Path)) { return $false }
    $normalized = Normalize-PathForMatch $Path
    return $normalized -match '(^|/)engine/src/tests/'
}

function Get-TestIdentityFromPath {
    param([string]$Path)
    if (-not (Test-IsFirstPartyTestPath $Path)) { return '' }
    return ConvertTo-TestIdentity ([IO.Path]::GetFileNameWithoutExtension($Path))
}

function Test-IsPromotionArmingPrompt {
    param([string]$Prompt)
    if ([string]::IsNullOrWhiteSpace($Prompt)) { return $false }
    if ($Prompt.Contains($script:PromotionArmingMarker)) { return $true }
    $trim = $Prompt.Trim()
    if ($trim -match '(?i)^/promote(\b|$)') { return $true }
    return $false
}

function Get-StopLoopLimit {
    param([bool]$Armed)
    if ($Armed) { return 4 }
    return 2
}

function Get-RepoRootFromPayload {
    param($Payload)
    $roots = Get-PayloadProperty $Payload 'workspace_roots'
    $rootList = @(ConvertTo-ObjectArray $roots)
    if ($rootList.Count -gt 0 -and -not [string]::IsNullOrWhiteSpace([string]$rootList[0])) {
        return [string]$rootList[0]
    }
    if ($PSScriptRoot) {
        return [IO.Path]::GetFullPath((Join-Path $PSScriptRoot (Join-Path '..' '..')))
    }
    return (Get-Location).Path
}

function Get-ConversationId {
    param($Payload)
    $id = Get-PayloadProperty $Payload 'conversation_id'
    if ($null -ne $id -and "$id" -ne '') {
        $safe = [string]$id
        $safe = $safe -replace '[^A-Za-z0-9._-]', '_'
        if ($safe.Length -gt 80) { $safe = $safe.Substring(0, 80) }
        return $safe
    }
    return 'default'
}

function Get-SessionStatePath {
    param(
        [string]$RepoRoot,
        [string]$ConversationId
    )
    $dir = Join-Path $RepoRoot (Join-Path '.cursor' 'agent-environment')
    return Join-Path $dir ($ConversationId + '.json')
}

function New-EmptySessionState {
    return [pscustomobject]@{
        edits       = @()
        successes   = @()
        testRuns    = @()
        armed       = $false
        armedTicks  = [int64]0
    }
}

function Get-SessionState {
    param([string]$Path)
    if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path)) {
        return (New-EmptySessionState)
    }
    try {
        $raw = Get-Content -LiteralPath $Path -Raw -ErrorAction Stop
        if ([string]::IsNullOrWhiteSpace($raw)) { return (New-EmptySessionState) }
        $parsed = $raw | ConvertFrom-Json
        $edits = @(ConvertTo-ObjectArray (Get-PayloadProperty $parsed 'edits'))
        $successes = @(ConvertTo-ObjectArray (Get-PayloadProperty $parsed 'successes'))
        $testRuns = @(ConvertTo-ObjectArray (Get-PayloadProperty $parsed 'testRuns'))
        $armed = $false
        $armedRaw = Get-PayloadProperty $parsed 'armed'
        if ($armedRaw -eq $true) { $armed = $true }
        $armedTicks = [int64]0
        $ticksRaw = Get-PayloadProperty $parsed 'armedTicks'
        if ($null -ne $ticksRaw -and "$ticksRaw" -ne '') {
            try { $armedTicks = [int64]$ticksRaw } catch { $armedTicks = [int64]0 }
        }
        return [pscustomobject]@{
            edits      = $edits
            successes  = $successes
            testRuns   = $testRuns
            armed      = $armed
            armedTicks = $armedTicks
        }
    } catch {
        return (New-EmptySessionState)
    }
}

function Save-SessionState {
    param(
        [string]$Path,
        $State
    )
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    $armed = $false
    $armedRaw = Get-PayloadProperty $State 'armed'
    if ($armedRaw -eq $true) { $armed = $true }
    $armedTicks = [int64]0
    $ticksRaw = Get-PayloadProperty $State 'armedTicks'
    if ($null -ne $ticksRaw -and "$ticksRaw" -ne '') {
        try { $armedTicks = [int64]$ticksRaw } catch { $armedTicks = [int64]0 }
    }
    $payload = [pscustomobject]@{
        edits      = @(ConvertTo-ObjectArray (Get-PayloadProperty $State 'edits'))
        successes  = @(ConvertTo-ObjectArray (Get-PayloadProperty $State 'successes'))
        testRuns   = @(ConvertTo-ObjectArray (Get-PayloadProperty $State 'testRuns'))
        armed      = $armed
        armedTicks = $armedTicks
    }
    $json = $payload | ConvertTo-Json -Depth 8 -Compress
    [IO.File]::WriteAllText($Path, $json)
}

function Update-SessionState {
    param(
        [string]$Path,
        [scriptblock]$Mutator,
        $Extra = $null
    )
    $dir = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $dir)) {
        New-Item -ItemType Directory -Path $dir -Force | Out-Null
    }
    $lockPath = $Path + '.lock'
    $attempt = 0
    while ($attempt -lt 8) {
        $attempt += 1
        $lockStream = $null
        try {
            $lockStream = [IO.File]::Open($lockPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
            $state = Get-SessionState $Path
            $next = & $Mutator $state $Extra
            if ($null -eq $next) { $next = $state }
            Save-SessionState -Path $Path -State $next
            return
        } catch {
            Start-Sleep -Milliseconds 40
        } finally {
            if ($null -ne $lockStream) {
                $lockStream.Close()
                $lockStream.Dispose()
            }
            if (Test-Path -LiteralPath $lockPath) {
                Remove-Item -LiteralPath $lockPath -Force -ErrorAction SilentlyContinue
            }
        }
    }
}

function Add-SessionEdit {
    param(
        $State,
        [string]$Path,
        [int64]$Ticks
    )
    $edits = @(ConvertTo-ObjectArray (Get-PayloadProperty $State 'edits'))
    $kept = @()
    foreach ($edit in $edits) {
        $existing = [string](Get-PayloadProperty $edit 'path')
        if ($existing -ne $Path) { $kept += $edit }
    }
    $kept += [pscustomobject]@{
        path  = $Path
        ticks = $Ticks
    }
    if ($kept.Count -gt 200) {
        $kept = $kept[($kept.Count - 200)..($kept.Count - 1)]
    }
    $State.edits = @($kept)
    return $State
}

function Add-SessionSuccess {
    param(
        $State,
        [string]$Command,
        [string]$Kind,
        [int64]$Ticks
    )
    $successes = @(ConvertTo-ObjectArray (Get-PayloadProperty $State 'successes'))
    $successes += [pscustomobject]@{
        command = $Command
        kind    = $Kind
        ticks   = $Ticks
    }
    if ($successes.Count -gt 80) {
        $successes = $successes[($successes.Count - 80)..($successes.Count - 1)]
    }
    $State.successes = @($successes)
    return $State
}

function Add-SessionTestRun {
    param(
        $State,
        [string]$Command,
        [bool]$Ok,
        [int64]$Ticks
    )
    $identities = Get-TestIdentityKeys $Command
    $idList = New-Object System.Collections.Generic.List[string]
    foreach ($id in (ConvertTo-ObjectList $identities)) {
        $idList.Add([string]$id)
    }
    $runs = @(ConvertTo-ObjectArray (Get-PayloadProperty $State 'testRuns'))
    $runs += [pscustomobject]@{
        command    = $Command
        ok         = $Ok
        ticks      = $Ticks
        identities = $idList.ToArray()
    }
    if ($runs.Count -gt 80) {
        $runs = $runs[($runs.Count - 80)..($runs.Count - 1)]
    }
    $State.testRuns = @($runs)
    return $State
}

function Set-SessionArmed {
    param(
        $State,
        [int64]$Ticks
    )
    $State.armed = $true
    $State.armedTicks = $Ticks
    return $State
}

function Test-IsSessionArmed {
    param($State)
    $armedRaw = Get-PayloadProperty $State 'armed'
    return ($armedRaw -eq $true)
}

function Test-HasPromotionEvidence {
    param($State)
    if (-not (Test-IsSessionArmed $State)) { return $false }
    $armedTicks = [int64]0
    $ticksRaw = Get-PayloadProperty $State 'armedTicks'
    if ($null -ne $ticksRaw -and "$ticksRaw" -ne '') {
        try { $armedTicks = [int64]$ticksRaw } catch { $armedTicks = [int64]0 }
    }
    $identityEditTicks = @{}
    foreach ($edit in (ConvertTo-ObjectList (Get-PayloadProperty $State 'edits'))) {
        $editTicks = [int64]0
        try { $editTicks = [int64](Get-PayloadProperty $edit 'ticks') } catch { $editTicks = [int64]0 }
        if ($editTicks -lt $armedTicks) { continue }
        $id = Get-TestIdentityFromPath ([string](Get-PayloadProperty $edit 'path'))
        if ($id -eq '') { continue }
        if (-not $identityEditTicks.ContainsKey($id) -or $editTicks -gt $identityEditTicks[$id]) {
            $identityEditTicks[$id] = $editTicks
        }
    }
    if ($identityEditTicks.Count -eq 0) { return $false }

    $runs = ConvertTo-ObjectList (Get-PayloadProperty $State 'testRuns')
    foreach ($id in $identityEditTicks.Keys) {
        $afterEdit = [int64]$identityEditTicks[$id]
        foreach ($run in $runs) {
            $ok = $false
            $okRaw = Get-PayloadProperty $run 'ok'
            if ($okRaw -eq $true) { $ok = $true }
            if ($ok) { continue }
            $redTicks = [int64]0
            try { $redTicks = [int64](Get-PayloadProperty $run 'ticks') } catch { $redTicks = [int64]0 }
            if ($redTicks -lt $afterEdit) { continue }
            $redIds = Get-PayloadProperty $run 'identities'
            if (-not (Test-TestIdentitiesOverlap @($id) $redIds)) { continue }
            foreach ($green in $runs) {
                $greenOk = $false
                $greenOkRaw = Get-PayloadProperty $green 'ok'
                if ($greenOkRaw -eq $true) { $greenOk = $true }
                if (-not $greenOk) { continue }
                $greenTicks = [int64]0
                try { $greenTicks = [int64](Get-PayloadProperty $green 'ticks') } catch { $greenTicks = [int64]0 }
                if ($greenTicks -lt $redTicks) { continue }
                $greenIds = Get-PayloadProperty $green 'identities'
                if (Test-TestIdentitiesOverlap @($id) $greenIds) { return $true }
            }
        }
    }
    return $false
}

function Get-PromotionEvidenceGap {
    param($State)
    if (-not (Test-IsSessionArmed $State)) { return $null }
    if (Test-HasPromotionEvidence $State) { return $null }
    $armedTicks = [int64]0
    $ticksRaw = Get-PayloadProperty $State 'armedTicks'
    if ($null -ne $ticksRaw -and "$ticksRaw" -ne '') {
        try { $armedTicks = [int64]$ticksRaw } catch { $armedTicks = [int64]0 }
    }
    $hasTestEdit = $false
    $identityEditTicks = @{}
    foreach ($edit in (ConvertTo-ObjectList (Get-PayloadProperty $State 'edits'))) {
        $editTicks = [int64]0
        try { $editTicks = [int64](Get-PayloadProperty $edit 'ticks') } catch { $editTicks = [int64]0 }
        if ($editTicks -lt $armedTicks) { continue }
        $id = Get-TestIdentityFromPath ([string](Get-PayloadProperty $edit 'path'))
        if ($id -eq '') { continue }
        $hasTestEdit = $true
        $identityEditTicks[$id] = $editTicks
    }
    if (-not $hasTestEdit) { return 'test_source' }

    $runs = ConvertTo-ObjectList (Get-PayloadProperty $State 'testRuns')
    $hasRed = $false
    foreach ($id in $identityEditTicks.Keys) {
        $afterEdit = [int64]$identityEditTicks[$id]
        foreach ($run in $runs) {
            $ok = $false
            $okRaw = Get-PayloadProperty $run 'ok'
            if ($okRaw -eq $true) { $ok = $true }
            if ($ok) { continue }
            $redTicks = [int64]0
            try { $redTicks = [int64](Get-PayloadProperty $run 'ticks') } catch { $redTicks = [int64]0 }
            if ($redTicks -lt $afterEdit) { continue }
            $redIds = Get-PayloadProperty $run 'identities'
            if (Test-TestIdentitiesOverlap @($id) $redIds) { $hasRed = $true }
        }
    }
    if (-not $hasRed) { return 'red' }
    return 'green'
}

function Get-FirstPartyTestNames {
    param([string]$RepoRoot)
    $names = New-Object System.Collections.Generic.List[string]
    $dir = Join-Path $RepoRoot (Join-Path 'engine' (Join-Path 'src' 'tests'))
    if (-not (Test-Path -LiteralPath $dir)) { return , $names }
    Get-ChildItem -LiteralPath $dir -File -ErrorAction SilentlyContinue | ForEach-Object {
        $names.Add($_.BaseName.ToLowerInvariant())
        $names.Add($_.Name.ToLowerInvariant())
    }
    return , $names
}

function Get-CoverageGaps {
    param(
        $Edits,
        $Successes,
        [string[]]$TestNames
    )
    $gaps = New-Object System.Collections.Generic.List[object]
    foreach ($edit in (ConvertTo-ObjectList $Edits)) {
        $path = [string](Get-PayloadProperty $edit 'path')
        $editTicks = 0
        try { $editTicks = [int64](Get-PayloadProperty $edit 'ticks') } catch { $editTicks = 0 }
        $stems = ConvertTo-ObjectList (Get-PathStems $path)
        $needsTest = Test-EditNeedsTestRun $stems $TestNames
        $covered = $false
        foreach ($success in (ConvertTo-ObjectList $Successes)) {
            $successTicks = 0
            try { $successTicks = [int64](Get-PayloadProperty $success 'ticks') } catch { $successTicks = 0 }
            if ($successTicks -lt $editTicks) { continue }
            $kind = [string](Get-PayloadProperty $success 'kind')
            $command = [string](Get-PayloadProperty $success 'command')
            if ($needsTest) {
                if ($kind -eq 'test_run' -and (Test-CommandContainsAnyStem $command $stems)) {
                    $covered = $true
                    break
                }
            } else {
                if ($kind -eq 'editor_build') {
                    $covered = $true
                    break
                }
            }
        }
        if (-not $covered) {
            $gaps.Add([pscustomobject]@{
                path      = $path
                stems     = $stems
                needsTest = $needsTest
            })
        }
    }
    return , $gaps
}

function New-CompletionFollowupMessage {
    param($Gaps)
    $gapList = ConvertTo-ObjectList $Gaps
    $lines = New-Object System.Collections.Generic.List[string]
    [void]$lines.Add('Completion evidence is missing for first-party C++ edits in this session. Claims in chat do not count. Run the commands in Shell. cmake --build with --target *_test is a build, not a Test run. Prefer ctest --output-on-failure so success can be observed.')
    [void]$lines.Add('')
    [void]$lines.Add('Uncovered:')
    $shown = 0
    foreach ($gap in $gapList) {
        if ($shown -ge 12) { break }
        $stems = ConvertTo-ObjectList (Get-PayloadProperty $gap 'stems')
        $stemText = if ($stems.Count -gt 0) { ($stems -join ', ') } else { '(none)' }
        $path = [string](Get-PayloadProperty $gap 'path')
        $needsTest = $false
        try { $needsTest = [bool](Get-PayloadProperty $gap 'needsTest') } catch { $needsTest = $false }
        if ($needsTest) {
            $pattern = ($stems -join '|')
            [void]$lines.Add("- $path (stems: $stemText) — need an observed Test run whose command contains a stem. Example: ctest --test-dir build/vs2026-debug -C Debug -R `"$pattern`" --output-on-failure")
        } else {
            [void]$lines.Add("- $path (stems: $stemText) — no matching name under engine/src/tests/; need an observed cmake --build … --target engine_editor")
        }
        $shown += 1
    }
    if ($gapList.Count -gt $shown) {
        [void]$lines.Add("...and $($gapList.Count - $shown) more.")
    }
    [void]$lines.Add('')
    [void]$lines.Add('See /validate and docs/agents/testing.md. Do not claim this session is verified until those Shell commands succeed.')
    return ($lines -join "`n")
}

function New-PromotionFollowupMessage {
    param([string]$GapKind)
    $lines = New-Object System.Collections.Generic.List[string]
    [void]$lines.Add('This session has Promotion arming. Promotion evidence is missing. Chat claims do not count. The Agent environment does not run tests.')
    [void]$lines.Add('')
    if ($GapKind -eq 'test_source') {
        [void]$lines.Add('Edit or add a first-party test under engine/src/tests/ that fails on the escaped defect (wire CMake if the target is new).')
    } elseif ($GapKind -eq 'red') {
        [void]$lines.Add('Run that test in Shell so a failing Test run is observed (RED). cmake --build with --target *_test is a build, not a Test run. Prefer ctest --output-on-failure.')
    } else {
        [void]$lines.Add('A failing Test run was observed. Fix the product code, then run the same test name in Shell until it passes (GREEN).')
    }
    [void]$lines.Add('One RED then GREEN pair on the same test name is enough for Promotion evidence. Phase 1 Completion evidence is still required for every first-party C++ edit.')
    return ($lines -join "`n")
}

function New-StopFollowupMessage {
    param(
        $Phase1Gaps,
        [string]$PromotionGapKind
    )
    $parts = New-Object System.Collections.Generic.List[string]
    if (-not [string]::IsNullOrWhiteSpace($PromotionGapKind)) {
        [void]$parts.Add((New-PromotionFollowupMessage $PromotionGapKind))
    }
    $gapList = ConvertTo-ObjectList $Phase1Gaps
    if ($gapList.Count -gt 0) {
        [void]$parts.Add((New-CompletionFollowupMessage $gapList))
    }
    return ($parts -join "`n`n")
}
