cmake_minimum_required(VERSION 3.23)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "Use an out-of-source build: cmake --preset debug")
endif()

project(cloudhouse_yuntai VERSION 1.0 LANGUAGES C ASM)

if(NOT CMAKE_C_COMPILER_ID STREQUAL "GNU" OR NOT CMAKE_CROSSCOMPILING)
    message(FATAL_ERROR "Use the ARM GCC toolchain: cmake --preset debug")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Explicit source lists follow the original Keil project. Only the startup
# assembly is replaced with a GNU assembler implementation.
add_executable(cloudhouse_yuntai
    Start/gcc/startup_stm32f103xb.S
    Start/gcc/newlib_support.c
    Start/core_cm3.c
    Start/system_stm32f10x.c
    User/main.c
    User/stm32f10x_it.c
    System/Delay.c
    System/Timer.c
    Hardware/App.c
    Hardware/LightSensor.c
    Hardware/LightTrack.c
    Hardware/LED.c
    Hardware/Key.c
    Hardware/OLED.c
    Hardware/PWM.c
    Hardware/Servo.c
    Hardware/MyI2C.c
    Hardware/MPU6050.c
    Hardware/Serial.c
    Hardware/OLED_Data.c
    Hardware/eMPL/inv_mpu.c
    Hardware/eMPL/inv_mpu_dmp_motion_driver.c
    Library/misc.c
    Library/stm32f10x_adc.c
    Library/stm32f10x_bkp.c
    Library/stm32f10x_can.c
    Library/stm32f10x_cec.c
    Library/stm32f10x_crc.c
    Library/stm32f10x_dac.c
    Library/stm32f10x_dbgmcu.c
    Library/stm32f10x_dma.c
    Library/stm32f10x_exti.c
    Library/stm32f10x_flash.c
    Library/stm32f10x_fsmc.c
    Library/stm32f10x_gpio.c
    Library/stm32f10x_i2c.c
    Library/stm32f10x_iwdg.c
    Library/stm32f10x_pwr.c
    Library/stm32f10x_rcc.c
    Library/stm32f10x_rtc.c
    Library/stm32f10x_sdio.c
    Library/stm32f10x_spi.c
    Library/stm32f10x_tim.c
    Library/stm32f10x_usart.c
    Library/stm32f10x_wwdg.c
)
set_target_properties(cloudhouse_yuntai PROPERTIES SUFFIX ".elf")

target_include_directories(cloudhouse_yuntai PRIVATE
    Start User Library System Hardware Hardware/eMPL
)
target_compile_definitions(cloudhouse_yuntai PRIVATE
    STM32F10X_MD
    USE_STDPERIPH_DRIVER
)
target_compile_options(cloudhouse_yuntai PRIVATE
    -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
    -ffunction-sections -fdata-sections -fno-common
    "$<$<COMPILE_LANGUAGE:C>:-Wall>"
    "$<$<CONFIG:Debug>:-Og;-g3>"
    "$<$<CONFIG:Release>:-Os>"
)
# The existing font tables intentionally use flat multidimensional initializers.
set_source_files_properties(Hardware/OLED_Data.c PROPERTIES
    COMPILE_OPTIONS -Wno-missing-braces
)

set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/linker/STM32F103C8Tx_FLASH.ld")
target_link_options(cloudhouse_yuntai PRIVATE
    -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
    -nostartfiles
    --specs=nano.specs --specs=nosys.specs
    "-T${LINKER_SCRIPT}"
    "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/cloudhouse_yuntai.map"
    -Wl,--gc-sections
    -Wl,--print-memory-usage
    -Wl,-z,noexecstack
    # The OLED displays angles with %f, so nano printf needs float support.
    -Wl,-u,_printf_float
)
target_link_libraries(cloudhouse_yuntai PRIVATE m)
set_property(TARGET cloudhouse_yuntai APPEND PROPERTY LINK_DEPENDS "${LINKER_SCRIPT}")

add_custom_command(TARGET cloudhouse_yuntai POST_BUILD
    COMMAND "${CMAKE_OBJCOPY}" -O ihex
        "$<TARGET_FILE:cloudhouse_yuntai>"
        "${CMAKE_CURRENT_BINARY_DIR}/cloudhouse_yuntai.hex"
    COMMAND "${CMAKE_OBJCOPY}" -O binary
        "$<TARGET_FILE:cloudhouse_yuntai>"
        "${CMAKE_CURRENT_BINARY_DIR}/cloudhouse_yuntai.bin"
    COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:cloudhouse_yuntai>"
    BYPRODUCTS
        "${CMAKE_CURRENT_BINARY_DIR}/cloudhouse_yuntai.hex"
        "${CMAKE_CURRENT_BINARY_DIR}/cloudhouse_yuntai.bin"
    COMMENT "Generating HEX/BIN firmware and reporting image size"
    VERBATIM
)
