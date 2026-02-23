param(
    [string]$Distro = "Ubuntu"
)

$ErrorActionPreference = "Stop"

Write-Host "[NeverMind] Preparing WSL distro: $Distro"
wsl -d $Distro -- bash -lc "sudo apt-get update"
wsl -d $Distro -- bash -lc "sudo apt-get install -y build-essential gcc clang lld binutils make grub-pc-bin grub-common xorriso qemu-system-x86 ovmf cppcheck clang-tools git ca-certificates"

Write-Host "[NeverMind] Environment ready in WSL."
Write-Host "Run in WSL project directory:"
Write-Host "  make clean all"
Write-Host "  make test"
Write-Host "  make integration"
Write-Host "  make user-tools"
Write-Host "  make smoke"
