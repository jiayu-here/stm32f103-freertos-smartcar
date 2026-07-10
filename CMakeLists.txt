cmake_minimum_required(VERSION 3.20)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_LIST_DIR}/cmake/arm-gcc-toolchain.cmake CACHE FILEPATH "")
project(stm32f103_freertos_smartcar C ASM)

set(DEVICE "C8" CACHE STRING "C8 or RCT6")
set_property(CACHE DEVICE PROPERTY STRINGS C8 RCT6)
if(DEVICE STREQUAL "C8")
  set(MCU_DEFINE STM32F103xB)
  set(STARTUP startup/startup_stm32f103xb.s)
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/STM32F103C8T6.ld)
elseif(DEVICE STREQUAL "RCT6")
  set(MCU_DEFINE STM32F103xE)
  set(STARTUP startup/startup_stm32f103xe.s)
  set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/linker/STM32F103RCT6.ld)
else()
  message(FATAL_ERROR "DEVICE must be C8 or RCT6")
endif()

file(GLOB HAL_SOURCES Drivers/STM32F1xx_HAL_Driver/Src/*.c)
list(FILTER HAL_SOURCES EXCLUDE REGEX "template|wwdg|usart|usb|spi|sram|smartcard|sd|rtc|pwr|pcd|pccard|nor|nand|mmc|irda|i2s|i2c|hcd|eth|dac|crc|cec|can")
set(SOURCES
  Core/Src/main.c Core/Src/stm32f1xx_it.c Core/Src/stm32f1xx_hal_msp.c Core/Src/syscalls.c
  App/Src/app.c App/Src/app_config.c App/Src/pid.c App/Src/protocol.c BSP/Src/bsp.c BSP/Src/oled.c
  Drivers/CMSIS/Device/ST/STM32F1xx/Source/Templates/system_stm32f1xx.c
  Middlewares/FreeRTOS/Source/tasks.c Middlewares/FreeRTOS/Source/queue.c
  Middlewares/FreeRTOS/Source/list.c Middlewares/FreeRTOS/Source/stream_buffer.c
  Middlewares/FreeRTOS/Source/portable/MemMang/heap_4.c
  Middlewares/FreeRTOS/Source/portable/GCC/ARM_CM3/port.c
  ${HAL_SOURCES} ${STARTUP})
add_executable(smartcar_${DEVICE}.elf ${SOURCES})
target_include_directories(smartcar_${DEVICE}.elf PRIVATE
  Core/Inc App/Inc BSP/Inc Drivers/STM32F1xx_HAL_Driver/Inc
  Drivers/STM32F1xx_HAL_Driver/Inc/Legacy Drivers/CMSIS/Include
  Drivers/CMSIS/Device/ST/STM32F1xx/Include Middlewares/FreeRTOS/Source/include
  Middlewares/FreeRTOS/Source/portable/GCC/ARM_CM3)
target_compile_definitions(smartcar_${DEVICE}.elf PRIVATE ${MCU_DEFINE} USE_HAL_DRIVER)
target_compile_options(smartcar_${DEVICE}.elf PRIVATE -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
  -Os -g3 -ffunction-sections -fdata-sections -Wall -Wextra)
target_link_options(smartcar_${DEVICE}.elf PRIVATE -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
  -T${LINKER_SCRIPT} --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections
  -Wl,--no-warn-rwx-segments -Wl,-Map=${CMAKE_BINARY_DIR}/smartcar_${DEVICE}.map -u _printf_float)
target_link_libraries(smartcar_${DEVICE}.elf m)
add_custom_command(TARGET smartcar_${DEVICE}.elf POST_BUILD
  COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:smartcar_${DEVICE}.elf> ${CMAKE_BINARY_DIR}/smartcar_${DEVICE}.hex
  COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:smartcar_${DEVICE}.elf> ${CMAKE_BINARY_DIR}/smartcar_${DEVICE}.bin
  COMMAND ${CMAKE_SIZE} $<TARGET_FILE:smartcar_${DEVICE}.elf>)
