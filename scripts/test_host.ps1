$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root 'build/host-tests'
New-Item -ItemType Directory -Force -Path $build | Out-Null
$compiler = Get-Command gcc -ErrorAction SilentlyContinue
if ($compiler) {
    $gcc = $compiler.Source
} else {
    $gcc = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Recurse `
        -Filter gcc.exe -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $gcc) { throw 'Host GCC not found. Install BrechtSanders.WinLibs.POSIX.UCRT with winget.' }
& $gcc -std=c11 -Wall -Wextra -Werror `
    (Join-Path $root 'tests/test_pid_protocol.c') `
    (Join-Path $root 'App/Src/pid.c') (Join-Path $root 'App/Src/protocol.c') `
    '-I' (Join-Path $root 'App/Inc') '-o' (Join-Path $build 'test_pid_protocol.exe')
if ($LASTEXITCODE -ne 0) { throw 'Host test compile failed' }
& (Join-Path $build 'test_pid_protocol.exe')
if ($LASTEXITCODE -ne 0) { throw 'Host tests failed' }
