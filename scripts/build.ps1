param(
    [ValidateSet('C8','RCT6')]
    [string]$Device = 'C8',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$toolBin = 'C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\14.2 rel1\bin'
$gcc = Join-Path $toolBin 'arm-none-eabi-gcc.exe'
$objcopy = Join-Path $toolBin 'arm-none-eabi-objcopy.exe'
$sizeTool = Join-Path $toolBin 'arm-none-eabi-size.exe'
if (-not (Test-Path -LiteralPath $gcc)) {
    $gcc = (Get-Command arm-none-eabi-gcc -ErrorAction Stop).Source
    $objcopy = (Get-Command arm-none-eabi-objcopy -ErrorAction Stop).Source
    $sizeTool = (Get-Command arm-none-eabi-size -ErrorAction Stop).Source
}

if ($Device -eq 'C8') {
    $define = 'STM32F103xB'
    $startup = 'startup/startup_stm32f103xb.s'
    $linker = 'linker/STM32F103C8T6.ld'
} else {
    $define = 'STM32F103xE'
    $startup = 'startup/startup_stm32f103xe.s'
    $linker = 'linker/STM32F103RCT6.ld'
}

$build = Join-Path $root "build/$Device"
if ($Clean -and (Test-Path -LiteralPath $build)) {
    Remove-Item -LiteralPath $build -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $build | Out-Null

$sources = @(
    'Core/Src/main.c', 'Core/Src/stm32f1xx_it.c', 'Core/Src/stm32f1xx_hal_msp.c',
    'Core/Src/syscalls.c',
    'App/Src/app.c', 'App/Src/app_config.c', 'App/Src/pid.c', 'App/Src/protocol.c',
    'BSP/Src/bsp.c', 'BSP/Src/oled.c',
    'Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_rcc_ex.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_gpio_ex.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_cortex.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_flash_ex.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_dma.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_tim_ex.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_uart.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_adc_ex.c',
    'Drivers/STM32F1xx_HAL_Driver/Src/stm32f1xx_hal_iwdg.c',
    'Middlewares/FreeRTOS/Source/tasks.c', 'Middlewares/FreeRTOS/Source/queue.c',
    'Middlewares/FreeRTOS/Source/list.c', 'Middlewares/FreeRTOS/Source/stream_buffer.c',
    'Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.c',
    'Middlewares/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c',
    $startup
)
$includes = @(
    'Core/Inc','App/Inc','BSP/Inc','Drivers/STM32F1xx_HAL_Driver/Inc',
    'Drivers/STM32F1xx_HAL_Driver/Inc/Legacy','Drivers/CMSIS/Include',
    'Drivers/CMSIS/Device/ST/STM32F1xx/Include','Middlewares/FreeRTOS/Source/include',
    'Middlewares/FreeRTOS/Source/portable/GCC/ARM_CM3'
)
$common = @('-mcpu=cortex-m3','-mthumb','-mfloat-abi=soft',"-D$define",'-DUSE_HAL_DRIVER')
$compile = $common + @('-std=gnu11','-Os','-g3','-ffunction-sections','-fdata-sections',
    '-Wall','-Wextra','-Werror=implicit-function-declaration')
foreach ($inc in $includes) { $compile += '-I' + (Join-Path $root $inc) }

$objects = @()
foreach ($source in $sources) {
    $sourcePath = Join-Path $root $source
    $objectName = ($source -replace '[\\/\.]','_') + '.o'
    $objectPath = Join-Path $build $objectName
    & $gcc @compile '-c' $sourcePath '-o' $objectPath
    if ($LASTEXITCODE -ne 0) { throw "Compile failed: $source" }
    $objects += $objectPath
}

$elf = Join-Path $build "smartcar_$Device.elf"
$map = Join-Path $build "smartcar_$Device.map"
$linkArgs = $common + @('-T', (Join-Path $root $linker), '--specs=nano.specs', '--specs=nosys.specs',
    '-Wl,--gc-sections', '-Wl,--no-warn-rwx-segments', "-Wl,-Map=$map", '-Wl,--cref',
    '-u','_printf_float') + $objects + @('-lm','-o',$elf)
& $gcc @linkArgs
if ($LASTEXITCODE -ne 0) { throw 'Link failed' }
& $objcopy '-O' 'ihex' $elf (Join-Path $build "smartcar_$Device.hex")
& $objcopy '-O' 'binary' $elf (Join-Path $build "smartcar_$Device.bin")
& $sizeTool $elf
Write-Host "BUILD OK: $elf"
