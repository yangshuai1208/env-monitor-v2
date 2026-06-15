$ErrorActionPreference = "Stop"

Write-Host "Start building STM32 project..."

if (Test-Path ".\Makefile") {
    make -j4
    exit $LASTEXITCODE
}

Write-Error "Makefile not found. Please generate Makefile project by STM32CubeMX first."
exit 1