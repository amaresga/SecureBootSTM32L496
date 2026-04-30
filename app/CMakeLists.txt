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
    # Generate raw binary — all header patching is done by tools/sign_image.py
    COMMAND arm-none-eabi-objcopy -O binary
            $<TARGET_FILE:App.elf>
            $<TARGET_FILE_DIR:App.elf>/App.bin
    COMMAND arm-none-eabi-size $<TARGET_FILE:App.elf>
    COMMENT "App: generating BIN — run tools/sign_image.py to sign before flashing"
)
