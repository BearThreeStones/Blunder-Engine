# Records first-party C++ edits for the Completion gate. Fail open.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'agent-environment-lib.ps1')

try {
    $inputText = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($inputText)) { exit 0 }

    $payload = $inputText | ConvertFrom-Json
    $repoRoot = Get-RepoRootFromPayload $payload
    $conversationId = Get-ConversationId $payload
    $statePath = Get-SessionStatePath $repoRoot $conversationId
    $ticks = [int64][DateTime]::UtcNow.Ticks

    foreach ($path in @(Get-PayloadFilePaths $payload)) {
        if (-not (Test-IsFirstPartyCppPath $path)) { continue }
        $relative = ConvertTo-RepoRelativePath $path $repoRoot
        if (-not (Test-IsFirstPartyCppPath $relative)) { continue }
        $extra = [pscustomobject]@{
            path  = $relative
            ticks = $ticks
        }
        Update-SessionState -Path $statePath -Mutator {
            param($State, $Extra)
            Add-SessionEdit $State ([string]$Extra.path) ([int64]$Extra.ticks)
        } -Extra $extra
    }
} catch {
}
exit 0
