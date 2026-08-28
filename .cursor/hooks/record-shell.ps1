# Records Test runs (pass or fail) and successful engine_editor builds. Fail open.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'agent-environment-lib.ps1')

try {
    $inputText = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($inputText)) { exit 0 }

    $payload = $inputText | ConvertFrom-Json
    $toolName = [string](Get-PayloadProperty $payload 'tool_name')
    $eventName = [string](Get-PayloadProperty $payload 'hook_event_name')
    $isShellEvent = $false
    if ($eventName -eq 'afterShellExecution') { $isShellEvent = $true }
    if ($toolName -eq 'Shell') { $isShellEvent = $true }
    if (-not $isShellEvent -and $eventName -ne '' -and $eventName -ne 'postToolUse') {
        exit 0
    }
    if (-not $isShellEvent -and $toolName -ne '' -and $toolName -ne 'Shell') {
        exit 0
    }

    $command = Get-PayloadCommand $payload
    $kind = Get-CommandKind $command
    if ([string]::IsNullOrWhiteSpace($kind)) { exit 0 }

    $output = Get-PayloadOutput $payload
    $ok = Test-ObservedSuccess $payload $command $output
    $failed = $false
    if ($kind -eq 'test_run') {
        $failed = Test-ObservedFailure $payload $command $output
    }
    if (-not $ok -and -not $failed) { exit 0 }

    $repoRoot = Get-RepoRootFromPayload $payload
    $conversationId = Get-ConversationId $payload
    $statePath = Get-SessionStatePath $repoRoot $conversationId
    $ticks = [int64][DateTime]::UtcNow.Ticks
    $extra = [pscustomobject]@{
        command        = $command
        kind           = $kind
        ticks          = $ticks
        ok             = $ok
        recordSuccess  = $ok
        recordTestRun  = ($kind -eq 'test_run')
    }
    Update-SessionState -Path $statePath -Mutator {
        param($State, $Extra)
        $next = $State
        if ($Extra.recordSuccess) {
            $next = Add-SessionSuccess $next ([string]$Extra.command) ([string]$Extra.kind) ([int64]$Extra.ticks)
        }
        if ($Extra.recordTestRun) {
            $next = Add-SessionTestRun $next ([string]$Extra.command) ([bool]$Extra.ok) ([int64]$Extra.ticks)
        }
        return $next
    } -Extra $extra
} catch {
}
exit 0
