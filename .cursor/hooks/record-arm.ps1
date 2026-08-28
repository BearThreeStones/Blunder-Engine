# Records Promotion arming from an explicit /promote (or equivalent) prompt. Fail open.
# Does not block the prompt.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'agent-environment-lib.ps1')

try {
    $inputText = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($inputText)) {
        Write-Output '{ "continue": true }'
        exit 0
    }

    $payload = $inputText | ConvertFrom-Json
    $prompt = [string](Get-PayloadProperty $payload 'prompt')
    if (Test-IsPromotionArmingPrompt $prompt) {
        $repoRoot = Get-RepoRootFromPayload $payload
        $conversationId = Get-ConversationId $payload
        $statePath = Get-SessionStatePath $repoRoot $conversationId
        $ticks = [int64][DateTime]::UtcNow.Ticks
        $extra = [pscustomobject]@{ ticks = $ticks }
        Update-SessionState -Path $statePath -Mutator {
            param($State, $Extra)
            Set-SessionArmed $State ([int64]$Extra.ticks)
        } -Extra $extra
    }
} catch {
}
Write-Output '{ "continue": true }'
exit 0
