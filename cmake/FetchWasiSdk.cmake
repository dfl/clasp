# FetchWasiSdk.cmake
# Downloads wasi-sdk for compiling C/C++ to WebAssembly

include(FetchContent)

set(WASI_SDK_VERSION "24" CACHE STRING "wasi-sdk version")

# Detect platform
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(WASI_SDK_PLATFORM "arm64-macos")
    else()
        set(WASI_SDK_PLATFORM "x86_64-macos")
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(WASI_SDK_PLATFORM "arm64-linux")
    else()
        set(WASI_SDK_PLATFORM "x86_64-linux")
    endif()
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
    set(WASI_SDK_PLATFORM "x86_64-windows")
else()
    message(FATAL_ERROR "Unsupported platform for wasi-sdk: ${CMAKE_HOST_SYSTEM_NAME}")
endif()

set(WASI_SDK_URL "https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-${WASI_SDK_VERSION}/wasi-sdk-${WASI_SDK_VERSION}.0-${WASI_SDK_PLATFORM}.tar.gz")

message(STATUS "Fetching wasi-sdk ${WASI_SDK_VERSION} for ${WASI_SDK_PLATFORM}")
message(STATUS "URL: ${WASI_SDK_URL}")

FetchContent_Declare(
    wasi_sdk
    URL ${WASI_SDK_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(wasi_sdk)

set(WASI_SDK_PREFIX "${wasi_sdk_SOURCE_DIR}" CACHE PATH "wasi-sdk installation prefix")
set(WASI_SDK_CLANG "${WASI_SDK_PREFIX}/bin/clang++" CACHE FILEPATH "wasi-sdk clang++")
set(WASI_SDK_SYSROOT "${WASI_SDK_PREFIX}/share/wasi-sysroot" CACHE PATH "wasi-sdk sysroot")

message(STATUS "wasi-sdk prefix: ${WASI_SDK_PREFIX}")
