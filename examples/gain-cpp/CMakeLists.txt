# Build WASM DSP module using wasi-sdk (auto-fetched)
cmake_minimum_required(VERSION 3.16)
project(gain-cpp-wasm NONE)  # No native compiler needed initially

# Fetch wasi-sdk
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/FetchWasiSdk.cmake)

# Now we can enable languages with wasi-sdk
set(CMAKE_C_COMPILER "${WASI_SDK_PREFIX}/bin/clang")
set(CMAKE_CXX_COMPILER "${WASI_SDK_PREFIX}/bin/clang++")
set(CMAKE_SYSROOT "${WASI_SDK_SYSROOT}")
set(CMAKE_CXX_STANDARD 17)

# Create the WASM target using custom command since CMake cross-compilation is tricky
set(WASM_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/gain.cpp")
set(WASM_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/dsp.wasm")

add_custom_command(
    OUTPUT ${WASM_OUTPUT}
    COMMAND ${WASI_SDK_PREFIX}/bin/clang++
        --target=wasm32-wasi
        --sysroot=${WASI_SDK_SYSROOT}
        -O3
        -msimd128
        -mbulk-memory
        -fno-exceptions
        -fno-rtti
        -nostdlib
        -Wl,--no-entry
        -Wl,--export-dynamic
        -o ${WASM_OUTPUT}
        ${WASM_SOURCE}
    DEPENDS ${WASM_SOURCE}
    COMMENT "Compiling gain.cpp to WebAssembly"
)

add_custom_target(dsp ALL DEPENDS ${WASM_OUTPUT})

# Copy plugin.json to build directory
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plugin.json
               ${CMAKE_CURRENT_BINARY_DIR}/plugin.json COPYONLY)

# Copy UI to build directory
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/ui DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Create .clasp bundle structure
set(CLASP_BUNDLE "${CMAKE_CURRENT_BINARY_DIR}/SimpleGain.clasp")
add_custom_command(
    TARGET dsp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CLASP_BUNDLE}
    COMMAND ${CMAKE_COMMAND} -E copy ${WASM_OUTPUT} ${CLASP_BUNDLE}/dsp.wasm
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/plugin.json ${CLASP_BUNDLE}/
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/ui ${CLASP_BUNDLE}/ui
    COMMENT "Creating SimpleGain.clasp bundle"
)

message(STATUS "Bundle will be created at: ${CLASP_BUNDLE}")
