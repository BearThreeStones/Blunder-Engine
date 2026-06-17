# Shared elevation helpers for nsight scripts (UAC + non-interactive detection).

function Test-NsightIsAdministrator {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Test-NsightCanPromptUac {
    if (-not [Environment]::UserInteractive) { return $false }
    # Cursor Agent / automation shells: interactive flag is true but UAC cannot be shown.
    if ($env:CURSOR_AGENT -eq '1') { return $false }
    if ($env:CI -eq 'true' -or $env:CI -eq '1') { return $false }
    if ([Console]::IsInputRedirected -and [Console]::IsOutputRedirected) { return $false }
    try {
        $cmd = (Get-CimInstance Win32_Process -Filter "ProcessId=$PID" -ErrorAction Stop).CommandLine
        if ($cmd -and $cmd -match '-NonInteractive') { return $false }
    } catch {
        return $false
    }
    return $true
}

function Write-NsightAdminRequiredMessage {
    param(
        [string]$RepoRoot,
        [string]$ExampleCommand
    )
    Write-Host ''
    Write-Host 'Administrator PowerShell required (Vulkan trace / nsys cleanup).' -ForegroundColor Yellow
    Write-Host 'UAC cannot be shown from this non-interactive terminal (e.g. Cursor Agent).' -ForegroundColor Yellow
    Write-Host ''
    Write-Host 'Open: Start -> Terminal / PowerShell -> Run as administrator, then:'
    Write-Host "  cd $RepoRoot"
    Write-Host "  $ExampleCommand"
    Write-Host ''
}

function Invoke-NsightElevation {
    param(
        [string]$RepoRoot,
        [string]$ExampleCommand,
        [string[]]$ElevateArgumentList
    )

    if (Test-NsightIsAdministrator) {
        return $true
    }

    if (-not (Test-NsightCanPromptUac)) {
        Write-NsightAdminRequiredMessage -RepoRoot $RepoRoot -ExampleCommand $ExampleCommand
        return $false
    }

    Write-Host 'Requesting Administrator (UAC) for Vulkan trace...'
    Start-Process powershell.exe -Verb RunAs -ArgumentList $ElevateArgumentList -Wait
    exit $LASTEXITCODE
}
