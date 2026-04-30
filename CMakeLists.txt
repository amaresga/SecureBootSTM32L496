cmake_minimum_required(VERSION 3.22)

# Toolchain must be set before project()
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-none-eabi.cmake)

project(SecureBootSTM32L496
    VERSION 0.1.0
    LANGUAGES C ASM
)

# Shared MCU flags propagated to both sub-targets via cache variable
set(STM32L496_MCU_FLAGS
    -mcpu=cortex-m4
    -mthumb
    -mfpu=fpv4-sp-d16
    -mfloat-abi=hard
    CACHE INTERNAL "Cortex-M4 hard-FP flags"
)

add_subdirectory(bootloader)
add_subdirectory(app)
