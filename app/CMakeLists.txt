# app/CMakeLists.txt
# Built by top-level umbrella — do NOT call project() or cmake_minimum_required() here.

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../ld/app.ld)

set(APP_SOURCES
    src/main.c
    src/image_header.c
    src/startup_stm32l496xx.s
    src/system_stm32l4xx.c
    # Shared boot infrastructure — referenced from bootloader directory
    ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/src/boot/boot.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../bootloader/src/boot/fault.c
)

add_executable(App.elf ${APP_SOURCES})

target_compile_options(App.elf PRIVATE
    ${STM32L496_MCU_FLAGS}
    -fdata-sections
    -ffunction-sections
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Release>:-O2>
)

target_link_options(App.elf PRIVATE
    ${STM32L496_MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,--no-warn-rwx-segments
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,-Map=$<TARGET_FILE_DIR:App.elf>/App.map
)

target_include_directories(App.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include/cmsis
)

add_custom_command(TARGET App.elf POST_BUILD
    # 1. Generate raw binary from ELF (address info stripped)
    COMMAND arm-none-eabi-objcopy -O binary
            $<TARGET_FILE:App.elf>
            $<TARGET_FILE_DIR:App.elf>/App.bin
    # 2. Patch length and CRC-32 into the image header
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/patch_image.py
            $<TARGET_FILE_DIR:App.elf>/App.bin
            0x08080000
    # 3. Regenerate .hex from the patched binary (restore load address)
    COMMAND arm-none-eabi-objcopy
            -I binary -O ihex
            --change-address 0x08080000
            $<TARGET_FILE_DIR:App.elf>/App.bin
            $<TARGET_FILE_DIR:App.elf>/App.hex
    COMMAND arm-none-eabi-size $<TARGET_FILE:App.elf>
    COMMENT "App: patching image header (length + CRC-32), generating HEX and BIN"
)
