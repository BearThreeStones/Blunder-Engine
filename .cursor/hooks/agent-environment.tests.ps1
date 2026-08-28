# Unit tests for Agent environment matching and coverage (no Pester).
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

. (Join-Path $PSScriptRoot 'agent-environment-lib.ps1')

$script:Failures = 0

function Assert-True {
    param(
        [bool]$Value,
        [string]$Message
    )
    if (-not $Value) {
        $script:Failures += 1
        Write-Host "FAIL $Message"
    }
}

function Assert-False {
    param(
        [bool]$Value,
        [string]$Message
    )
    Assert-True (-not $Value) $Message
}

function Assert-Eq {
    param(
        $Actual,
        $Expected,
        [string]$Message
    )
    if ("$Actual" -ne "$Expected") {
        $script:Failures += 1
        Write-Host "FAIL $Message : [$Actual] vs [$Expected]"
    }
}

function ConvertTo-TestArray {
    param($Value)
    $out = New-Object System.Collections.Generic.List[object]
    if ($null -eq $Value) { return , $out }
    if ($Value -is [string]) {
        $out.Add($Value)
        return , $out
    }
    foreach ($item in $Value) {
        $out.Add($item)
    }
    return , $out
}

function Get-TestCount {
    param($Value)
    return (ConvertTo-TestArray $Value).Count
}

function Assert-ArrayEq {
    param(
        $Actual,
        $Expected,
        [string]$Message
    )
    $a = ConvertTo-TestArray $Actual
    $e = ConvertTo-TestArray $Expected
    if ($a.Count -ne $e.Count) {
        $script:Failures += 1
        Write-Host "FAIL $Message count: [$($a -join ',')] vs [$($e -join ',')]"
        return
    }
    for ($i = 0; $i -lt $e.Count; $i++) {
        if ("$($a[$i])" -ne "$($e[$i])") {
            $script:Failures += 1
            Write-Host "FAIL $Message : [$($a -join ',')] vs [$($e -join ',')]"
            return
        }
    }
}

Assert-ArrayEq (Get-PathStems 'engine/src/runtime/function/editor/hierarchy_system.cpp') @('hierarchy') 'hierarchy_system stems'
Assert-ArrayEq (Get-PathStems 'engine/src/runtime/function/scene/scene_instance.cpp') @('scene', 'instance') 'scene_instance stems'
Assert-ArrayEq (Get-PathStems 'engine/src/runtime/core/log/log_system.h') @('log') 'log_system stems'
Assert-ArrayEq (Get-PathStems 'transform_gizmo_controller.cpp') @('transform', 'gizmo') 'controller dropped'
Assert-ArrayEq (Get-PathStems 'foo-bar.cpp') @('foo', 'bar') 'hyphen split'
Assert-ArrayEq (Get-PathStems 'system.cpp') @() 'all-generic stems'

Assert-True (Test-IsFirstPartyCppPath 'engine/src/runtime/foo.cpp') 'first-party cpp'
Assert-True (Test-IsFirstPartyCppPath 'E:/Dev/Blunder-Engine/engine/src/tests/hierarchy_create_test.cpp') 'first-party test cpp'
Assert-False (Test-IsFirstPartyCppPath 'engine/src/runtime/function/slint/editor_window.slint') 'slint not armed'
Assert-False (Test-IsFirstPartyCppPath 'engine/3rdparty/glm/glm.cpp') 'vendor not armed'
Assert-False (Test-IsFirstPartyCppPath 'docs/agents/testing.md') 'docs not armed'
Assert-False (Test-IsFirstPartyCppPath 'CMakeLists.txt') 'cmake not armed'

Assert-True (Test-IsTestRun 'ctest --test-dir build/vs2026-debug -C Debug -R hierarchy --output-on-failure') 'ctest is Test run'
Assert-True (Test-IsTestRun '.\build\vs2026-debug\engine\src\tests\Debug\hierarchy_create_test.exe') 'test exe is Test run'
Assert-True (Test-IsTestRun './classdb_test') 'unix test exe is Test run'
Assert-True (Test-IsTestRun 'cmake --build build/vs2026-debug --config Debug --target classdb_test ; .\classdb_test.exe') 'compound build+exe is Test run'
Assert-False (Test-IsTestRun 'cmake --build build/vs2026-debug --config Debug --target classdb_test') 'test-target compile is not Test run'
Assert-False (Test-IsTestRun 'cmake --build build/vs2026-debug --config Debug --target engine_editor') 'editor build is not Test run'

Assert-True (Test-IsEditorBuild 'cmake --build build/vs2026-debug --config Debug --target engine_editor') 'editor cmake build'
Assert-False (Test-IsEditorBuild 'cmake --build build/vs2026-debug --config Debug --target classdb_test') 'test target is not editor build'
Assert-False (Test-IsEditorBuild 'ctest -R engine_editor') 'ctest is not editor build'

$testNames = @('hierarchy_create_test', 'hierarchy_line_test', 'classdb_test', 'scene_serializer_test')

Assert-True (Test-EditNeedsTestRun @('hierarchy') $testNames) 'hierarchy has tests'
Assert-False (Test-EditNeedsTestRun @('uniquewidget') $testNames) 'uniquewidget has no tests'

$editHierarchy = [pscustomobject]@{ path = 'engine/src/runtime/function/editor/hierarchy_system.cpp'; ticks = 10 }
$editUnique = [pscustomobject]@{ path = 'engine/src/runtime/function/editor/unique_widget.cpp'; ticks = 10 }
$testRun = [pscustomobject]@{ command = 'ctest --test-dir build/vs2026-debug -C Debug -R hierarchy --output-on-failure'; kind = 'test_run'; ticks = 20 }
$unrelatedTest = [pscustomobject]@{ command = 'ctest -R classdb --output-on-failure'; kind = 'test_run'; ticks = 20 }
$editorBuild = [pscustomobject]@{ command = 'cmake --build build/vs2026-debug --config Debug --target engine_editor'; kind = 'editor_build'; ticks = 20 }
$earlyTest = [pscustomobject]@{ command = 'ctest -R hierarchy --output-on-failure'; kind = 'test_run'; ticks = 5 }

$covered = Get-CoverageGaps @($editHierarchy) @($testRun) $testNames
Assert-Eq (Get-TestCount $covered) 0 'hierarchy test run covers edit'

$notCoveredByEditor = Get-CoverageGaps @($editHierarchy) @($editorBuild) $testNames
Assert-Eq (Get-TestCount $notCoveredByEditor) 1 'editor build does not cover file with matching tests'

$notCoveredByUnrelated = Get-CoverageGaps @($editHierarchy) @($unrelatedTest) $testNames
Assert-Eq (Get-TestCount $notCoveredByUnrelated) 1 'unrelated test run does not cover hierarchy'

$lateEdit = [pscustomobject]@{ path = 'engine/src/runtime/function/editor/hierarchy_system.cpp'; ticks = 30 }
$after = Get-CoverageGaps @($lateEdit) @($testRun) $testNames
Assert-Eq (Get-TestCount $after) 1 'earlier test run does not cover later edit'

$uniqueCovered = Get-CoverageGaps @($editUnique) @($editorBuild) $testNames
Assert-Eq (Get-TestCount $uniqueCovered) 0 'editor build covers file with no matching tests'

$uniqueNotByTest = Get-CoverageGaps @($editUnique) @($testRun) $testNames
Assert-Eq (Get-TestCount $uniqueNotByTest) 1 'unrelated Test run does not cover no-test file'

$tooEarly = Get-CoverageGaps @($editHierarchy) @($earlyTest) $testNames
Assert-Eq (Get-TestCount $tooEarly) 1 'test run before edit does not cover'

$ctestOk = @{ output = "100% tests passed, 0 tests failed out of 2`n" }
Assert-True (Test-ObservedSuccess $ctestOk 'ctest -R hierarchy --output-on-failure' $ctestOk.output) 'ctest 100% is success'
$ctestFail = @{ output = "The following tests FAILED:`n`t1 - hierarchy_create_test`n" }
Assert-False (Test-ObservedSuccess $ctestFail 'ctest -R hierarchy' $ctestFail.output) 'ctest FAILED is not success'
$ctestNone = @{ output = 'No tests were found!!!' }
Assert-False (Test-ObservedSuccess $ctestNone 'ctest -R instance' $ctestNone.output) 'no tests found is not success'
$buildOk = @{ output = "Build succeeded.`n    0 Warning(s)`n    0 Error(s)`n" }
Assert-True (Test-ObservedSuccess $buildOk 'cmake --build build --config Debug --target engine_editor' $buildOk.output) 'MSVC build succeeded'
$buildFail = @{ output = 'Build FAILED.' }
Assert-False (Test-ObservedSuccess $buildFail 'cmake --build build --config Debug --target engine_editor' $buildFail.output) 'MSVC build failed'

$shellPayload = [pscustomobject]@{
    tool_output = '{"exitCode":0,"stdout":""}'
    tool_input  = [pscustomobject]@{ command = '.\hierarchy_create_test.exe' }
}
Assert-Eq (Get-PayloadExitCode $shellPayload) 0 'parse postToolUse exitCode'
Assert-True (Test-ObservedSuccess $shellPayload (Get-PayloadCommand $shellPayload) (Get-PayloadOutput $shellPayload)) 'exitCode 0 counts with empty stdout'

$failedPayload = [pscustomobject]@{
    tool_output = '{"exitCode":1,"stdout":"boom"}'
    tool_input  = [pscustomobject]@{ command = '.\hierarchy_create_test.exe' }
}
Assert-False (Test-ObservedSuccess $failedPayload (Get-PayloadCommand $failedPayload) (Get-PayloadOutput $failedPayload)) 'exitCode 1 is not success'

$unknownTest = @{ output = '' }
Assert-False (Test-ObservedSuccess $unknownTest '.\hierarchy_create_test.exe' '') 'silent test exe without exit code is not success'

Assert-False (Test-ObservedSuccess @{ output = "0% tests passed, 1 tests failed out of 1`n" } 'ctest -R hierarchy' "0% tests passed, 1 tests failed out of 1`n") 'ctest 0% is not success'

Assert-Eq (ConvertTo-TestIdentity 'hierarchy_line_test.exe') 'hierarchy_line' 'strip _test and .exe'
Assert-Eq (ConvertTo-TestIdentity 'hierarchy_line') 'hierarchy_line' 'ctest -R token stays'
Assert-True (Test-TestIdentitiesOverlap (Get-TestIdentityKeys 'ctest -R hierarchy_line --output-on-failure') (Get-TestIdentityKeys '.\hierarchy_line_test.exe')) 'ctest -R name pairs with exe'
Assert-False (Test-TestIdentitiesOverlap (Get-TestIdentityKeys 'ctest -R hierarchy --output-on-failure') (Get-TestIdentityKeys '.\hierarchy_line_test.exe')) 'shared stem is not the same test'
Assert-False (Test-TestIdentitiesOverlap (Get-TestIdentityKeys 'ctest -R hierarchy_create') (Get-TestIdentityKeys 'ctest -R hierarchy_line')) 'different hierarchy_* tests do not pair'
Assert-True (Test-IsFirstPartyTestPath 'engine/src/tests/hierarchy_line_test.cpp') 'tests dir is test source'
Assert-False (Test-IsFirstPartyTestPath 'engine/src/runtime/function/editor/hierarchy_system.cpp') 'product cpp is not test source'
Assert-Eq (Get-TestIdentityFromPath 'engine/src/tests/hierarchy_line_test.cpp') 'hierarchy_line' 'identity from test path'

Assert-True (Test-IsPromotionArmingPrompt "<!-- BLUNDER_PROMOTION_ARMING -->`nThis session is Failure promotion.") 'marker arms'
Assert-True (Test-IsPromotionArmingPrompt "/promote") 'slash command arms'
Assert-False (Test-IsPromotionArmingPrompt 'Please promote this bug in chat.') 'prose does not arm'
Assert-Eq (Get-StopLoopLimit $false) 2 'unarmed loop limit'
Assert-Eq (Get-StopLoopLimit $true) 4 'armed loop limit'

$failOut = "The following tests FAILED:`n`t1 - hierarchy_line_test`n"
Assert-True (Test-ObservedFailure @{ output = $failOut } 'ctest -R hierarchy_line' $failOut) 'ctest FAILED is RED'
Assert-False (Test-ObservedFailure @{ output = 'No tests were found!!!' } 'ctest -R instance' 'No tests were found!!!') 'no tests found is not RED'
Assert-False (Test-ObservedFailure @{ output = "100% tests passed, 0 tests failed out of 1`n" } 'ctest -R hierarchy_line' "100% tests passed, 0 tests failed out of 1`n") '100% is not RED'
$failPayload = [pscustomobject]@{
    tool_output = '{"exitCode":1,"stdout":"boom"}'
    tool_input  = [pscustomobject]@{ command = '.\hierarchy_line_test.exe' }
}
Assert-True (Test-ObservedFailure $failPayload (Get-PayloadCommand $failPayload) (Get-PayloadOutput $failPayload)) 'exitCode 1 is RED'

$promoState = New-EmptySessionState
$promoState = Set-SessionArmed $promoState 10
Assert-True (Test-IsSessionArmed $promoState) 'arming sets armed'
Assert-False (Test-HasPromotionEvidence $promoState) 'armed without test edit is not evidence'
Assert-Eq (Get-PromotionEvidenceGap $promoState) 'test_source' 'gap is test source'
$promoState = Add-SessionEdit $promoState 'engine/src/tests/hierarchy_line_test.cpp' 20
Assert-Eq (Get-PromotionEvidenceGap $promoState) 'red' 'gap is RED after test edit'
$promoState = Add-SessionTestRun $promoState 'ctest -R hierarchy_line --output-on-failure' $false 30
Assert-Eq (Get-PromotionEvidenceGap $promoState) 'green' 'gap is GREEN after RED'
$promoState = Add-SessionTestRun $promoState '.\hierarchy_line_test.exe' $true 40
Assert-True (Test-HasPromotionEvidence $promoState) 'RED then GREEN of same test name'
Assert-Eq (Get-PromotionEvidenceGap $promoState) $null 'no promotion gap when pair exists'

$sabotage = New-EmptySessionState
$sabotage = Set-SessionArmed $sabotage 10
$sabotage = Add-SessionEdit $sabotage 'engine/src/tests/classdb_test.cpp' 20
$sabotage = Add-SessionTestRun $sabotage 'ctest -R classdb_test' $false 30
$sabotage = Add-SessionTestRun $sabotage 'ctest -R classdb_test' $true 40
$sabotage = Add-SessionEdit $sabotage 'engine/src/runtime/function/editor/hierarchy_system.cpp' 25
Assert-True (Test-HasPromotionEvidence $sabotage) 'pair on the test that was edited after arming'
$wrongPair = New-EmptySessionState
$wrongPair = Set-SessionArmed $wrongPair 10
$wrongPair = Add-SessionEdit $wrongPair 'engine/src/tests/hierarchy_line_test.cpp' 20
$wrongPair = Add-SessionTestRun $wrongPair 'ctest -R classdb_test' $false 30
$wrongPair = Add-SessionTestRun $wrongPair 'ctest -R classdb_test' $true 40
Assert-False (Test-HasPromotionEvidence $wrongPair) 'RED-GREEN of unedited test name does not count'

$earlyGreen = New-EmptySessionState
$earlyGreen = Set-SessionArmed $earlyGreen 10
$earlyGreen = Add-SessionEdit $earlyGreen 'engine/src/tests/hierarchy_line_test.cpp' 20
$earlyGreen = Add-SessionTestRun $earlyGreen 'ctest -R hierarchy_line' $true 25
$earlyGreen = Add-SessionTestRun $earlyGreen 'ctest -R hierarchy_line' $false 30
Assert-False (Test-HasPromotionEvidence $earlyGreen) 'GREEN before RED does not count'


$tempRoot = Join-Path $env:TEMP ('blunder-agent-env-tests-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot | Out-Null
try {
    $statePath = Get-SessionStatePath $tempRoot 'conv-test'
    $state = New-EmptySessionState
    $state = Add-SessionEdit $state 'engine/src/foo.cpp' 10
    $state = Add-SessionEdit $state 'engine/src/foo.cpp' 30
    $state = Add-SessionSuccess $state 'ctest -R foo' 'test_run' 20
    Save-SessionState $statePath $state
    $loaded = Get-SessionState $statePath
    Assert-Eq (Get-TestCount (ConvertTo-ObjectArray $loaded.edits)) 1 'upsert latest edit per path'
    Assert-Eq ([int64](Get-PayloadProperty @(ConvertTo-ObjectArray $loaded.edits)[0] 'ticks')) 30 'kept later ticks'
} finally {
    Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

$hookRoot = $PSScriptRoot
$recordEdit = Join-Path $hookRoot 'record-edit.ps1'
$recordShell = Join-Path $hookRoot 'record-shell.ps1'
$recordArm = Join-Path $hookRoot 'record-arm.ps1'
$protectPaths = Join-Path $hookRoot 'protect-paths.ps1'
$gate = Join-Path $hookRoot 'completion-gate.ps1'
$fakeRepo = Join-Path $env:TEMP ('blunder-agent-env-hooks-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $fakeRepo 'engine\src\tests') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $fakeRepo 'engine\src\runtime\function\editor') -Force | Out-Null
Set-Content -LiteralPath (Join-Path $fakeRepo 'engine\src\tests\hierarchy_create_test.cpp') -Value '// fixture'
Set-Content -LiteralPath (Join-Path $fakeRepo 'engine\src\tests\hierarchy_line_test.cpp') -Value '// fixture'
Set-Content -LiteralPath (Join-Path $fakeRepo 'engine\src\runtime\function\editor\hierarchy_system.cpp') -Value '// fixture'
$repoJson = ($fakeRepo -replace '\\', '/')

function Invoke-HookFile {
    param(
        [string]$ScriptPath,
        [string]$Json
    )
    $tempIn = Join-Path $env:TEMP ('hook-in-' + [guid]::NewGuid().ToString('N') + '.json')
    [IO.File]::WriteAllText($tempIn, $Json)
    try {
        $out = Get-Content -LiteralPath $tempIn -Raw | & powershell -NoProfile -ExecutionPolicy Bypass -File $ScriptPath
        return ($out | Out-String).Trim()
    } finally {
        Remove-Item -LiteralPath $tempIn -Force -ErrorAction SilentlyContinue
    }
}

$protectAllow = Invoke-HookFile $protectPaths '{"tool_name":"Write","tool_input":{"path":"CONTEXT.md"}}'
Assert-True ($protectAllow -match '"permission": "allow"') 'protect-paths allows first-party docs'
$protectDeny = Invoke-HookFile $protectPaths '{"tool_name":"Write","tool_input":{"path":"engine/3rdparty/glm/glm.hpp"}}'
Assert-True ($protectDeny -match '"permission": "deny"') 'protect-paths denies vendor'
$protectSlint = Invoke-HookFile $protectPaths '{"tool_name":"Write","tool_input":{"path":"engine/3rdparty/slint/api/foo.cpp"}}'
Assert-True ($protectSlint -match '"permission": "allow"') 'protect-paths allows Slint fork'

try {
    $editJson = @"
{"conversation_id":"hook-itest","workspace_roots":["$repoJson"],"file_path":"$repoJson/engine/src/runtime/function/editor/hierarchy_system.cpp"}
"@
    [void](Invoke-HookFile $recordEdit $editJson)

    $stopJson = @"
{"conversation_id":"hook-itest","workspace_roots":["$repoJson"],"status":"completed","loop_count":0}
"@
    $gateOut = Invoke-HookFile $gate $stopJson
    Assert-True ($gateOut -match 'followup_message') 'stop follow-up when evidence missing'
    Assert-True ($gateOut -match 'hierarchy') 'follow-up names uncovered stem'

    $abortJson = @"
{"conversation_id":"hook-itest","workspace_roots":["$repoJson"],"status":"aborted","loop_count":0}
"@
    Assert-Eq (Invoke-HookFile $gate $abortJson) '{}' 'aborted stop does not follow up'

    $loopedJson = @"
{"conversation_id":"hook-itest","workspace_roots":["$repoJson"],"status":"completed","loop_count":2}
"@
    Assert-Eq (Invoke-HookFile $gate $loopedJson) '{}' 'loop_count 2 does not follow up'

    $buildOnly = @"
{"conversation_id":"hook-itest","workspace_roots":["$repoJson"],"hook_event_name":"afterShellExecution","command":"cmake --build build/vs2026-debug --config Debug --target hierarchy_create_test","output":"Build succeeded."}
"@
    [void](Invoke-HookFile $recordShell $buildOnly)
    $stillGated = Invoke-HookFile $gate $stopJson
    Assert-True ($stillGated -match 'followup_message') 'test-target compile does not satisfy the gate'

    $testRunJson = @"
{"conversation_id":"hook-itest","workspace_roots":["$repoJson"],"hook_event_name":"afterShellExecution","command":"ctest --test-dir build/vs2026-debug -C Debug -R hierarchy --output-on-failure","output":"100% tests passed, 0 tests failed out of 1"}
"@
    [void](Invoke-HookFile $recordShell $testRunJson)
    $cleared = Invoke-HookFile $gate $stopJson
    Assert-Eq $cleared '{}' 'observed Test run clears the gate'

    $armJson = @"
{"conversation_id":"hook-promote","workspace_roots":["$repoJson"],"prompt":"<!-- BLUNDER_PROMOTION_ARMING -->\nPromote escaped defect"}
"@
    $armOut = Invoke-HookFile $recordArm $armJson
    Assert-True ($armOut -match 'continue') 'arming hook does not block the prompt'

    $promoStop = @"
{"conversation_id":"hook-promote","workspace_roots":["$repoJson"],"status":"completed","loop_count":0}
"@
    $promoGate = Invoke-HookFile $gate $promoStop
    Assert-True ($promoGate -match 'followup_message') 'armed session follow-up without Promotion evidence'
    Assert-True ($promoGate -match 'Promotion arming') 'follow-up names Promotion arming'

    $armedLoop2 = @"
{"conversation_id":"hook-promote","workspace_roots":["$repoJson"],"status":"completed","loop_count":2}
"@
    $stillArmed = Invoke-HookFile $gate $armedLoop2
    Assert-True ($stillArmed -match 'followup_message') 'armed loop_count 2 still follow-up'

    $testEditJson = @"
{"conversation_id":"hook-promote","workspace_roots":["$repoJson"],"file_path":"$repoJson/engine/src/tests/hierarchy_line_test.cpp"}
"@
    [void](Invoke-HookFile $recordEdit $testEditJson)
    $needRed = Invoke-HookFile $gate $promoStop
    Assert-True ($needRed -match 'failing Test run') 'after test edit, ask for RED'

    $redJson = @"
{"conversation_id":"hook-promote","workspace_roots":["$repoJson"],"hook_event_name":"afterShellExecution","command":"ctest --test-dir build/vs2026-debug -C Debug -R hierarchy_line --output-on-failure","output":"The following tests FAILED:\n\t1 - hierarchy_line_test"}
"@
    [void](Invoke-HookFile $recordShell $redJson)
    $needGreen = Invoke-HookFile $gate $promoStop
    Assert-True ($needGreen -match 'GREEN') 'after RED, ask for GREEN'

    $greenJson = @"
{"conversation_id":"hook-promote","workspace_roots":["$repoJson"],"hook_event_name":"afterShellExecution","command":".\\hierarchy_line_test.exe","tool_output":"{\"exitCode\":0,\"stdout\":\"\"}"}
"@
    [void](Invoke-HookFile $recordShell $greenJson)
    $promoCleared = Invoke-HookFile $gate $promoStop
    Assert-Eq $promoCleared '{}' 'RED then GREEN of same test name clears Promotion evidence'

    $armedLoop4 = @"
{"conversation_id":"hook-promote","workspace_roots":["$repoJson"],"status":"completed","loop_count":4}
"@
    Assert-Eq (Invoke-HookFile $gate $armedLoop4) '{}' 'armed loop_count 4 does not follow up'
} finally {
    Remove-Item -LiteralPath $fakeRepo -Recurse -Force -ErrorAction SilentlyContinue
}

if ($script:Failures -gt 0) {
    Write-Host "agent-environment tests failed: $script:Failures"
    exit 1
}
Write-Host 'agent-environment tests passed'
exit 0
