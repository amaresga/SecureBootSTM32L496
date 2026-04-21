cmake_minimum_required(VERSION 3.22)

# Toolchain must be set before project()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)

project(STM32L496_Boot
    VERSION 0.1.0
    LANGUAGES C ASM
)

# MCU flags
set(MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
)

# Linker script
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/ld/STM32L496ZGTx_FLASH.ld)

# Compiler flags shared across targets
add_compile_options(
    ${MCU_FLAGS}
    -fdata-sections
    -ffunction-sections
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Release>:-O2>
)

add_link_options(
    ${MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,--no-warn-rwx-segments
    --specs=nano.specs
    --specs=nosys.specs
)

# Sources
set(SOURCES
    src/startup_stm32l496xx.s
    src/boot/boot.c
    src/boot/fault.c
    src/main.c
    src/system_stm32l4xx.c
)

add_executable(${PROJECT_NAME}.elf ${SOURCES})

target_include_directories(${PROJECT_NAME}.elf PRIVATE
    include
    include/cmsis
)

# Per-config map file (must be on the target, not add_link_options, for generator expr support)
target_link_options(${PROJECT_NAME}.elf PRIVATE
    -Wl,-Map=$<TARGET_FILE_DIR:${PROJECT_NAME}.elf>/${PROJECT_NAME}.map
)

# Generate .bin and .hex after build
add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O ihex   $<TARGET_FILE:${PROJECT_NAME}.elf> $<TARGET_FILE_DIR:${PROJECT_NAME}.elf>/${PROJECT_NAME}.hex
    COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:${PROJECT_NAME}.elf> $<TARGET_FILE_DIR:${PROJECT_NAME}.elf>/${PROJECT_NAME}.bin
    COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}.elf>
    COMMENT "Generating HEX and BIN"
)
