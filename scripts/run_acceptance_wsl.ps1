param(
    [string]$Distro = ""
)

$ErrorActionPreference = "Stop"

$useDefault = [string]::IsNullOrWhiteSpace($Distro)
$winPath = (Get-Location).Path

if ($useDefault) {
    $wslPath = (wsl -- wslpath -a "$winPath").Trim()
}
else {
    $wslPath = (wsl -d $Distro -- wslpath -a "$winPath").Trim()
}

if ($LASTEXITCODE -ne 0) {
    throw "Failed to resolve WSL path. Check WSL installation and distro availability."
}

if ([string]::IsNullOrWhiteSpace($wslPath)) {
    throw "Cannot resolve WSL path for $winPath"
}

if ($useDefault) {
    Write-Host "[NeverMind] Running acceptance in default WSL distro"
}
else {
    Write-Host "[NeverMind] Running acceptance in WSL distro '$Distro'"
}
Write-Host "[NeverMind] Workspace: $wslPath"

if ($useDefault) {
    wsl -- bash -lc "cd '$wslPath' && chmod +x tests/run_full_acceptance.sh && make acceptance"
}
else {
    wsl -d $Distro -- bash -lc "cd '$wslPath' && chmod +x tests/run_full_acceptance.sh && make acceptance"
}

if ($LASTEXITCODE -ne 0) {
    throw "Acceptance failed in WSL. See terminal output for details."
}

Write-Host "[NeverMind] Done. Check tests/results-YYYYMMDD/summary.txt"
