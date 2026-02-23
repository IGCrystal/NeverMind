$ErrorActionPreference = "Stop"

$dateTag = Get-Date -Format "yyyyMMdd"
$resultDir = "tests/results-$dateTag"
$summaryFile = Join-Path $resultDir "summary.txt"
$buildLog = Join-Path $resultDir "build.log"

New-Item -ItemType Directory -Force -Path $resultDir | Out-Null
Set-Content -Path $summaryFile -Value "NeverMind Acceptance Summary ($dateTag)`n======================================"
Set-Content -Path $buildLog -Value ""

function Add-Summary([string]$line) {
    Add-Content -Path $summaryFile -Value $line
}

function Invoke-Step([string]$name, [string]$command) {
    Add-Content -Path $buildLog -Value "[$(Get-Date -Format HH:mm:ss)] BEGIN $name"
    try {
        & powershell -NoProfile -Command $command *>> $buildLog
        Add-Summary "PASS: $name"
    }
    catch {
        Add-Summary "FAIL: $name"
        throw
    }
    finally {
        Add-Content -Path $buildLog -Value "[$(Get-Date -Format HH:mm:ss)] END $name"
    }
}

Invoke-Step "clean+build" "make clean all"
Invoke-Step "unit-tests" "make test"
Invoke-Step "integration-tests" "make integration"
Invoke-Step "userspace-tools" "make user-tools"
Invoke-Step "smoke-tests" "make smoke"

if (Test-Path "build/test-logs") {
    Get-ChildItem "build/test-logs/*.log" -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $_.FullName -Destination $resultDir -Force
    }
}

Add-Content -Path $summaryFile -Value ""
Add-Content -Path $summaryFile -Value "Artifacts:"
Add-Content -Path $summaryFile -Value "- Build log: $buildLog"
Add-Content -Path $summaryFile -Value "- Kernel image: build/kernel.elf"
Add-Content -Path $summaryFile -Value "- ISO image: build/nevermind-m1.iso"

Write-Host "Acceptance completed. See $summaryFile"
