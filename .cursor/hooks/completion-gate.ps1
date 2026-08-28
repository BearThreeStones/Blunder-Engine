# Completion gate: stop follow-up when Completion evidence or Promotion evidence is missing. Fail open.
# Does not run CTest or any other command.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'agent-environment-lib.ps1')

function Write-EmptyStop {
    Write-Output '{}'
    exit 0
}

try {
    $inputText = [Console]::In.ReadToEnd()
    if ([string]::IsNullOrWhiteSpace($inputText)) { Write-EmptyStop }

    $payload = $inputText | ConvertFrom-Json
    $status = [string](Get-PayloadProperty $payload 'status')
    if ($status -ne '' -and $status -ne 'completed') { Write-EmptyStop }

    $repoRoot = Get-RepoRootFromPayload $payload
    $conversationId = Get-ConversationId $payload
    $statePath = Get-SessionStatePath $repoRoot $conversationId
    $state = Get-SessionState $statePath
    $armed = Test-IsSessionArmed $state

    $loopCount = 0
    $rawLoop = Get-PayloadProperty $payload 'loop_count'
    if ($null -ne $rawLoop -and "$rawLoop" -ne '') {
        try { $loopCount = [int]$rawLoop } catch { $loopCount = 0 }
    }
    if ($loopCount -ge (Get-StopLoopLimit $armed)) { Write-EmptyStop }

    $editList = ConvertTo-ObjectList (Get-PayloadProperty $state 'edits')
    $testNames = Get-FirstPartyTestNames $repoRoot
    $successes = ConvertTo-ObjectList (Get-PayloadProperty $state 'successes')
    $phase1Gaps = New-Object System.Collections.Generic.List[object]
    if ($editList.Count -gt 0) {
        $phase1Gaps = Get-CoverageGaps $editList $successes $testNames
    }

    $promotionGap = $null
    if ($armed) {
        $promotionGap = Get-PromotionEvidenceGap $state
    }

    $needPhase1 = $phase1Gaps.Count -gt 0
    $needPromo = -not [string]::IsNullOrWhiteSpace([string]$promotionGap)
    if (-not $needPhase1 -and -not $needPromo) { Write-EmptyStop }

    $message = New-StopFollowupMessage $phase1Gaps $promotionGap
    $result = [pscustomobject]@{ followup_message = $message }
    Write-Output ($result | ConvertTo-Json -Compress)
    exit 0
} catch {
    Write-Output '{}'
    exit 0
}
