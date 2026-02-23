param(
    [string]$Distro = "",
    [switch]$FixWslConf
)

$ErrorActionPreference = "Stop"

function Invoke-WslBash([string]$bashCmd) {
    if ([string]::IsNullOrWhiteSpace($Distro)) {
        wsl -- bash -lc $bashCmd
    }
    else {
        wsl -d $Distro -- bash -lc $bashCmd
    }

    if ($LASTEXITCODE -ne 0) {
        throw "WSL command failed: $bashCmd"
    }
}

if ($FixWslConf) {
    if ([string]::IsNullOrWhiteSpace($Distro)) {
        Write-Host "[NeverMind] Fixing /etc/wsl.conf in default distro (if needed)"
    }
    else {
        Write-Host "[NeverMind] Fixing /etc/wsl.conf in distro '$Distro' (if needed)"
    }

    Invoke-WslBash "if [ -f /etc/wsl.conf ]; then if grep -q '^[[:space:]]*wsl2\.kernelCommandLine[[:space:]]*=' /etc/wsl.conf; then sudo cp /etc/wsl.conf /etc/wsl.conf.bak.nevermind.`$(date +%s); sudo sed -i '/^[[:space:]]*wsl2\.kernelCommandLine[[:space:]]*=/d' /etc/wsl.conf; echo '[NeverMind] Removed invalid key: wsl2.kernelCommandLine'; else echo '[NeverMind] No invalid wsl2.kernelCommandLine key found'; fi; else echo '[NeverMind] /etc/wsl.conf not present'; fi"
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
Invoke-WslBash "missing=\"\"; for p in $pkgList; do dpkg -s \"\$p\" >/dev/null 2>&1 || missing=\"\$missing \$p\"; done; if [ -n \"\$missing\" ]; then echo \"[NeverMind] Installing missing packages:\$missing\"; sudo apt-get update; sudo apt-get install -y \$missing; else echo \"[NeverMind] All required packages already installed.\"; fi"

Write-Host "[NeverMind] Environment ready in WSL."
Write-Host "Run in WSL project directory:"
Write-Host "  make clean all"
Write-Host "  make test"
Write-Host "  make integration"
Write-Host "  make user-tools"
Write-Host "  make smoke"
Write-Host "  make acceptance"
