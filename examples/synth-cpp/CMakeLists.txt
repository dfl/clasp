# Build WASM DSP module for synth using wasi-sdk
cmake_minimum_required(VERSION 3.16)
project(synth-cpp-wasm LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_executable(dsp src/synth.cpp)

target_compile_options(dsp PRIVATE
    -O3
    -msimd128
    -mbulk-memory
    -fno-exceptions
    -fno-rtti
)

target_link_options(dsp PRIVATE
    -Wl,--no-entry
    -Wl,--export-dynamic
    -nostdlib
)

set_target_properties(dsp PROPERTIES
    SUFFIX ".wasm"
    OUTPUT_NAME "dsp"
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/plugin.json
               ${CMAKE_CURRENT_BINARY_DIR}/plugin.json COPYONLY)
