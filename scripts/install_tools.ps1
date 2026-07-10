$ErrorActionPreference = 'Stop'

if (-not (Get-Command arm-none-eabi-gcc -ErrorAction SilentlyContinue)) {
    throw 'Arm GNU Toolchain is missing. Install Arm GNU Toolchain 14.x and reopen PowerShell.'
}
$pio = 'C:\Users\jiayu\.platformio\penv\Scripts\platformio.exe'
if (-not (Test-Path -LiteralPath $pio)) {
    py -m pip install --user platformio
    $pio = (Get-Command platformio -ErrorAction Stop).Source
}
& $pio pkg install --global --tool platformio/tool-openocd
py -m pip install --user pyserial
Write-Host 'Tools ready: Arm GCC, OpenOCD, and pyserial.'
