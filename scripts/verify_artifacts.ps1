$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$sumFile = Join-Path $root 'artifacts/SHA256SUMS.txt'
foreach ($line in Get-Content -LiteralPath $sumFile) {
    if ([string]::IsNullOrWhiteSpace($line)) { continue }
    $parts = $line -split '\s+', 2
    $expected = $parts[0].ToLowerInvariant()
    $relative = $parts[1] -replace '/', '\'
    $path = Join-Path (Join-Path $root 'artifacts') $relative
    $actual = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $expected) { throw "Checksum mismatch: $relative" }
    Write-Host "OK $relative"
}
Write-Host 'PASS: prebuilt firmware checksums'
