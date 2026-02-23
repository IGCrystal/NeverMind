param(
    [string]$Distro = ""
)

$ErrorActionPreference = "Stop"

function Invoke-WslBash([string]$bashCmd, [bool]$throwOnFail = $true) {
    if ([string]::IsNullOrWhiteSpace($Distro)) {
        wsl -- bash -lc $bashCmd
    }
    else {
        wsl -d $Distro -- bash -lc $bashCmd
    }

    if ($throwOnFail -and $LASTEXITCODE -ne 0) {
        throw "WSL command failed: $bashCmd"
    }

    return $LASTEXITCODE
}

$useDefault = [string]::IsNullOrWhiteSpace($Distro)
$winPath = (Get-Location).Path

$drive = $winPath.Substring(0, 1).ToLower()
$tail = ($winPath.Substring(2) -replace '\\', '/')
$wslPath = "/mnt/$drive$tail"

if ($useDefault) {
    Write-Host "[NeverMind] Running acceptance in default WSL distro"
}
else {
    Write-Host "[NeverMind] Running acceptance in WSL distro '$Distro'"
}
Write-Host "[NeverMind] Workspace: $wslPath"

$hasMake = Invoke-WslBash "command -v make >/dev/null 2>&1" $false
if ($hasMake -ne 0) {
    throw "make not found in WSL. Run: powershell -ExecutionPolicy Bypass -File .\\scripts\\setup_wsl.ps1"
}

Invoke-WslBash "cd '$wslPath' && chmod +x tests/run_full_acceptance.sh && make acceptance"

Write-Host "[NeverMind] Done. Check tests/results-YYYYMMDD/summary.txt"
