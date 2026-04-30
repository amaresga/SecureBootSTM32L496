# shared/crypto/CMakeLists.txt
# Builds a static Ed25519 verification library backed by Monocypher 3.1.3.
# Only included by bootloader/CMakeLists.txt.
#
# Monocypher sources must be vendored before building:
#   .\shared\crypto\setup_crypto.ps1
#
# The script downloads the Monocypher 3.1.3 archive from GitHub using
# Windows' native TLS (Invoke-WebRequest) and extracts the required sources
# to shared/crypto/vendor/monocypher-3.1.3/.

set(MONOCYPHER_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/monocypher-3.1.3")

if(NOT EXISTS "${MONOCYPHER_VENDOR_DIR}/src/monocypher.c")
    message(FATAL_ERROR
        "Monocypher sources not found in ${MONOCYPHER_VENDOR_DIR}\n"
        "Run the one-time setup script first:\n"
        "    powershell -ExecutionPolicy Bypass -File shared/crypto/setup_crypto.ps1"
    )
endif()

add_library(crypto_ed25519 STATIC
    ed25519_verify.c
    ${MONOCYPHER_VENDOR_DIR}/src/monocypher.c
    ${MONOCYPHER_VENDOR_DIR}/src/optional/monocypher-ed25519.c
)

target_compile_options(crypto_ed25519 PRIVATE
    ${STM32L496_MCU_FLAGS}
    -ffunction-sections
    -fdata-sections
    -Os
    -w           # suppress third-party warnings
)

target_include_directories(crypto_ed25519
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}                           # ed25519_verify.h
    PRIVATE ${MONOCYPHER_VENDOR_DIR}/src                          # monocypher.h
            ${MONOCYPHER_VENDOR_DIR}/src/optional                 # monocypher-ed25519.h
)
