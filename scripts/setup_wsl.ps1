param(
    [string]$Distro = "",
    [switch]$FixWslConf
)

$ErrorActionPreference = "Stop"

function Invoke-WslBash([string]$bashCmd) {
    $bashCmd = $bashCmd -replace "`r", ""
    $bashBytes = [System.Text.Encoding]::UTF8.GetBytes($bashCmd)
    $bashB64 = [System.Convert]::ToBase64String($bashBytes)
    $runner = "echo '$bashB64' | base64 -d | bash"

    if ([string]::IsNullOrWhiteSpace($Distro)) {
        wsl -- bash -lc $runner
    }
    else {
        wsl -d $Distro -- bash -lc $runner
    }

    if ($LASTEXITCODE -ne 0) {
        throw "WSL command failed: $bashCmd"
    }
}

function Invoke-WslBashTemplate([string]$template, [hashtable]$replacements) {
    $cmd = $template
    foreach ($key in $replacements.Keys) {
        $cmd = $cmd.Replace($key, $replacements[$key])
    }
    Invoke-WslBash $cmd
}

if ($FixWslConf) {
    if ([string]::IsNullOrWhiteSpace($Distro)) {
        Write-Host "[NeverMind] Fixing /etc/wsl.conf in default distro (if needed)"
    }
    else {
        Write-Host "[NeverMind] Fixing /etc/wsl.conf in distro '$Distro' (if needed)"
    }

        $fixWslConfCmd = @'
set -euo pipefail
if [ -f /etc/wsl.conf ]; then
    if grep -Eq '^[[:space:]]*kernelCommandLine[[:space:]]*=' /etc/wsl.conf; then
        sudo cp /etc/wsl.conf /etc/wsl.conf.bak.nevermind.$(date +%s)
        sudo sed -Ei '/^[[:space:]]*kernelCommandLine[[:space:]]*=/d' /etc/wsl.conf
        echo '[NeverMind] Removed invalid key: kernelCommandLine'
    else
        echo '[NeverMind] No invalid kernelCommandLine key found'
    fi
else
    echo '[NeverMind] /etc/wsl.conf not present'
fi
'@
        Invoke-WslBash $fixWslConfCmd
    Write-Host "[NeverMind] If changes were applied, run 'wsl --shutdown' once from Windows." 
}

if ([string]::IsNullOrWhiteSpace($Distro)) {
    Write-Host "[NeverMind] Preparing default WSL distro"
}
else {
    Write-Host "[NeverMind] Preparing WSL distro: $Distro"
}

$packages = @(
    "build-essential", "gcc", "clang", "lld", "binutils", "make",
    "grub-pc-bin", "grub-common", "xorriso",
    "qemu-system-x86", "ovmf",
    "cppcheck", "clang-tools",
    "git", "ca-certificates"
)

$pkgList = ($packages -join " ")
$installCmdTemplate = @'
set -euo pipefail
missing=""
for p in __PKG_LIST__; do
    dpkg -s "$p" >/dev/null 2>&1 || missing="$missing $p"
done
if [ -n "$missing" ]; then
    echo "[NeverMind] Installing missing packages:$missing"
    sudo apt-get update
    sudo apt-get install -y $missing
else
    echo "[NeverMind] All required packages already installed."
fi
'@
Invoke-WslBashTemplate $installCmdTemplate @{"__PKG_LIST__" = $pkgList}

Write-Host "[NeverMind] Environment ready in WSL."
Write-Host "Run in WSL project directory:"
Write-Host "  make clean all"
Write-Host "  make test"
Write-Host "  make integration"
Write-Host "  make user-tools"
Write-Host "  make smoke"
Write-Host "  make acceptance"
