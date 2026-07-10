$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'test_host.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
py (Join-Path $PSScriptRoot 'validate_project.py')
if ($LASTEXITCODE -ne 0) { throw 'Project validation failed' }
& (Join-Path $PSScriptRoot 'build.ps1') -Device C8 -Clean
if ($LASTEXITCODE -ne 0) { throw 'C8 build failed' }
& (Join-Path $PSScriptRoot 'build.ps1') -Device RCT6 -Clean
if ($LASTEXITCODE -ne 0) { throw 'RCT6 build failed' }
& (Join-Path $PSScriptRoot 'verify_artifacts.ps1')
if ($LASTEXITCODE -ne 0) { throw 'Artifact verification failed' }
Write-Host 'PASS: all tests and both firmware targets'
