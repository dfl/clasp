# FetchWasmtime.cmake
# Downloads and configures the Wasmtime C API for the current platform

include(FetchContent)

# Wasmtime version to use
set(WASMTIME_VERSION "27.0.0")

# Determine platform and architecture
if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(WASMTIME_ARCH "aarch64")
    else()
        set(WASMTIME_ARCH "x86_64")
    endif()
    set(WASMTIME_PLATFORM "macos")
    set(WASMTIME_EXT "tar.xz")
elseif(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(WASMTIME_ARCH "x86_64")
    else()
        set(WASMTIME_ARCH "i686")
    endif()
    set(WASMTIME_PLATFORM "windows")
    set(WASMTIME_EXT "zip")
elseif(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
        set(WASMTIME_ARCH "aarch64")
    else()
        set(WASMTIME_ARCH "x86_64")
    endif()
    set(WASMTIME_PLATFORM "linux")
    set(WASMTIME_EXT "tar.xz")
else()
    message(FATAL_ERROR "Unsupported platform for Wasmtime")
endif()

set(WASMTIME_NAME "wasmtime-v${WASMTIME_VERSION}-${WASMTIME_ARCH}-${WASMTIME_PLATFORM}-c-api")
set(WASMTIME_URL "https://github.com/bytecodealliance/wasmtime/releases/download/v${WASMTIME_VERSION}/${WASMTIME_NAME}.${WASMTIME_EXT}")

message(STATUS "Fetching Wasmtime ${WASMTIME_VERSION} for ${WASMTIME_PLATFORM}-${WASMTIME_ARCH}")
message(STATUS "URL: ${WASMTIME_URL}")

FetchContent_Declare(
    wasmtime
    URL ${WASMTIME_URL}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(wasmtime)

# Set variables for use in parent CMakeLists.txt
set(WASMTIME_INCLUDE_DIR "${wasmtime_SOURCE_DIR}/include" CACHE PATH "Wasmtime include directory")

if(WIN32)
    set(WASMTIME_LIBRARIES "${wasmtime_SOURCE_DIR}/lib/wasmtime.dll.lib" CACHE FILEPATH "Wasmtime library")
    set(WASMTIME_DLL "${wasmtime_SOURCE_DIR}/lib/wasmtime.dll" CACHE FILEPATH "Wasmtime DLL")
elseif(APPLE)
    set(WASMTIME_LIBRARIES "${wasmtime_SOURCE_DIR}/lib/libwasmtime.a" CACHE FILEPATH "Wasmtime library")
else()
    set(WASMTIME_LIBRARIES "${wasmtime_SOURCE_DIR}/lib/libwasmtime.a" CACHE FILEPATH "Wasmtime library")
endif()

message(STATUS "Wasmtime include: ${WASMTIME_INCLUDE_DIR}")
message(STATUS "Wasmtime library: ${WASMTIME_LIBRARIES}")
