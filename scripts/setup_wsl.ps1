param(
    [string]$Distro = ""
)

$ErrorActionPreference = "Stop"

$useDefault = [string]::IsNullOrWhiteSpace($Distro)
if ($useDefault) {
    Write-Host "[NeverMind] Preparing default WSL distro"
    wsl -- bash -lc "sudo apt-get update"
    if ($LASTEXITCODE -ne 0) { throw "wsl apt-get update failed" }
    wsl -- bash -lc "sudo apt-get install -y build-essential gcc clang lld binutils make grub-pc-bin grub-common xorriso qemu-system-x86 ovmf cppcheck clang-tools git ca-certificates"
    if ($LASTEXITCODE -ne 0) { throw "wsl package install failed" }
}
else {
    Write-Host "[NeverMind] Preparing WSL distro: $Distro"
    wsl -d $Distro -- bash -lc "sudo apt-get update"
    if ($LASTEXITCODE -ne 0) { throw "wsl apt-get update failed for distro '$Distro'" }
    wsl -d $Distro -- bash -lc "sudo apt-get install -y build-essential gcc clang lld binutils make grub-pc-bin grub-common xorriso qemu-system-x86 ovmf cppcheck clang-tools git ca-certificates"
    if ($LASTEXITCODE -ne 0) { throw "wsl package install failed for distro '$Distro'" }
}

Write-Host "[NeverMind] Environment ready in WSL."
Write-Host "Run in WSL project directory:"
Write-Host "  make clean all"
Write-Host "  make test"
Write-Host "  make integration"
Write-Host "  make user-tools"
Write-Host "  make smoke"
