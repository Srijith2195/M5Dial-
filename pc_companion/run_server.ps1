<#
.SYNOPSIS
    Starts the M5Dial PC Companion Server on Windows 11.
#>

$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$rootDir = Split-Path -Parent $scriptDir

# Check if .env exists, otherwise warn
$envFile = Join-Path $scriptDir ".env"
if (!(Test-Path $envFile)) {
    $rootEnv = Join-Path $rootDir ".env"
    if (!(Test-Path $rootEnv)) {
        Write-Host "Notice: No .env file found. Copying .env.example to .env..." -ForegroundColor Yellow
        Copy-Item (Join-Path $scriptDir ".env.example") $envFile
    }
}

# Find local IPv4 address
$ip = (Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.InterfaceAlias -notmatch 'Loopback' -and $_.IPAddress -notmatch '^169\.254\.' } | Select-Object -First 1).IPAddress
if (!$ip) {
    $ip = (ipconfig | Select-String "IPv4 Address" | Select-Object -First 1) -replace '.*?:\s*', ''
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Starting M5Dial PC Control Center Companion Server     " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Detected PC IP : $ip" -ForegroundColor Green
Write-Host "Starting Python server..." -ForegroundColor Green
Write-Host ""

Set-Location $rootDir
python (Join-Path $scriptDir "server.py")
