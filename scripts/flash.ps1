param(
    [ValidateSet('C8','RCT6')]
    [string]$Device = 'C8'
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$image = Join-Path $root "build/$Device/smartcar_$Device.bin"
if (-not (Test-Path -LiteralPath $image)) {
    & (Join-Path $PSScriptRoot 'build.ps1') -Device $Device
}

$programmer = 'C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe'
$openocd = 'C:\Users\jiayu\.platformio\packages\tool-openocd\bin\openocd.exe'
if (Test-Path -LiteralPath $programmer) {
    & $programmer -c port=SWD -w $image 0x08000000 -v -rst
} elseif (Test-Path -LiteralPath $openocd) {
    $scripts = Join-Path (Split-Path -Parent $openocd) '..\share\openocd\scripts'
    & $openocd -s $scripts -f interface/stlink.cfg -f target/stm32f1x.cfg `
        -c "adapter speed 1000" -c "program {$image} verify reset exit 0x08000000"
} else {
    throw 'STM32CubeProgrammer or PlatformIO OpenOCD not found. Run scripts/install_tools.ps1 first.'
}
if ($LASTEXITCODE -ne 0) { throw 'Flash/verify failed. Check ST-Link, target power, and SWD wiring.' }
