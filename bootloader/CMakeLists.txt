# bootloader/CMakeLists.txt
# Built by top-level umbrella — do NOT call project() or cmake_minimum_required() here.

set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/../ld/bootloader.ld)

# Ed25519 verification library (Monocypher, downloaded at configure time)
add_subdirectory(${CMAKE_SOURCE_DIR}/shared/crypto ${CMAKE_BINARY_DIR}/shared/crypto)

set(BOOTLOADER_SOURCES
    src/startup_stm32l496xx.s
    src/system_stm32l4xx.c
    src/boot/boot.c
    src/boot/fault.c
    src/boot/pubkey.c
    src/boot/verify.c
    src/main.c
)

add_executable(Bootloader.elf ${BOOTLOADER_SOURCES})

target_compile_options(Bootloader.elf PRIVATE
    ${STM32L496_MCU_FLAGS}
    -fdata-sections
    -ffunction-sections
    -Wall
    -Wextra
    $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Debug>:-Og>
    $<$<CONFIG:Release>:-O2>
)

target_link_options(Bootloader.elf PRIVATE
    ${STM32L496_MCU_FLAGS}
    -T${LINKER_SCRIPT}
    -Wl,--gc-sections
    -Wl,--no-warn-rwx-segments
    --specs=nano.specs
    --specs=nosys.specs
    -Wl,-Map=$<TARGET_FILE_DIR:Bootloader.elf>/Bootloader.map
)

target_include_directories(Bootloader.elf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/include/cmsis
    ${CMAKE_CURRENT_SOURCE_DIR}/src/boot   # pubkey.h
)

target_link_libraries(Bootloader.elf PRIVATE crypto_ed25519)

add_custom_command(TARGET Bootloader.elf POST_BUILD
    COMMAND arm-none-eabi-objcopy -O ihex   $<TARGET_FILE:Bootloader.elf> $<TARGET_FILE_DIR:Bootloader.elf>/Bootloader.hex
    COMMAND arm-none-eabi-objcopy -O binary $<TARGET_FILE:Bootloader.elf> $<TARGET_FILE_DIR:Bootloader.elf>/Bootloader.bin
    COMMAND arm-none-eabi-size $<TARGET_FILE:Bootloader.elf>
    COMMENT "Bootloader: generating HEX, BIN, and size"
)
