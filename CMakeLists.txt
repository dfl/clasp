cmake_minimum_required(VERSION 3.21)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CLASP_VERSION_FULL)
string(STRIP "${CLASP_VERSION_FULL}" CLASP_VERSION_FULL)
# Extract numeric version (before any -alpha, -beta, etc.)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CLASP_VERSION "${CLASP_VERSION_FULL}")

project(clasp VERSION ${CLASP_VERSION} LANGUAGES CXX C)
message(STATUS "CLASP version: ${CLASP_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options

# Include our cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Fetch Wasmtime
include(FetchWasmtime)

# Add external dependencies as submodules
add_subdirectory(extern/clap EXCLUDE_FROM_ALL)
add_subdirectory(extern/clap-helpers EXCLUDE_FROM_ALL)

# CHOC is header-only, just need to add include path
set(CHOC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/choc")

# wclap-cpp is header-only
set(WCLAP_CPP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/wclap-cpp/include")

# clasp-gui for WebView support
set(CHOC_DIR "${CHOC_INCLUDE_DIR}")
set(CLAP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/clap")
add_subdirectory(extern/clasp-gui EXCLUDE_FROM_ALL)

# clasp library (shared code)
set(CLASP_CORE_SOURCES
    src/wclap_runtime.cpp
    src/scanner.cpp
    src/params.cpp
    src/state.cpp
    src/gui.cpp
    src/logger.cpp
)

add_library(clasp_core STATIC ${CLASP_CORE_SOURCES})

target_include_directories(clasp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CHOC_INCLUDE_DIR}
    ${WCLAP_CPP_INCLUDE_DIR}
    ${WASMTIME_INCLUDE_DIR}
)

target_link_libraries(clasp_core PUBLIC
    clap
    clap-helpers
    clasp-gui
    ${WASMTIME_LIBRARIES}
)

# Suppress GCC warnings from wclap-cpp headers (they use C-style struct naming)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(clasp_core PRIVATE -Wno-changes-meaning)
endif()

# thunder.clap plugin (dev-oriented WCLAP host with hot-reload)
add_library(thunder MODULE
    src/plugin_entry.cpp
    src/plugin_factory.cpp
    src/plugin_instance.cpp
)

target_link_libraries(thunder PRIVATE clasp_core)

# Suppress GCC warnings from wclap-cpp headers
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(thunder PRIVATE -Wno-changes-meaning)
endif()

# Set plugin output properties
set_target_properties(thunder PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    OUTPUT_NAME "thunder"
    BUNDLE TRUE
    BUNDLE_EXTENSION "clap"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.clasp.thunder"
    MACOSX_BUNDLE_BUNDLE_NAME "thunder"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# macOS: Build as proper bundle
if(APPLE)
    set_target_properties(thunder PROPERTIES
        MACOSX_BUNDLE TRUE
        XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
    )
endif()

# Install target
if(APPLE)
    set(CLAP_INSTALL_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/CLAP")
elseif(WIN32)
    set(CLAP_INSTALL_DIR "$ENV{COMMONPROGRAMFILES}/CLAP")
else()
    set(CLAP_INSTALL_DIR "$ENV{HOME}/.clap")
endif()

install(TARGETS thunder
    LIBRARY DESTINATION "${CLAP_INSTALL_DIR}"
    BUNDLE DESTINATION "${CLAP_INSTALL_DIR}"
)

# Examples (WCLAP plugins are built separately with wasi-sdk/cargo)
# See templates/ for WCLAP project templates

# Testing
option(CLASP_BUILD_TESTS "Build unit tests" OFF)
if(CLASP_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
      Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG        v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)

    add_executable(clasp_tests
        tests/scanner_test.cpp
    )
    target_link_libraries(clasp_tests PRIVATE
        clasp_core
        Catch2::Catch2WithMain
    )

    if(WIN32)
        add_custom_command(TARGET clasp_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${WASMTIME_DLL}"
            "$<TARGET_FILE_DIR:clasp_tests>/wasmtime.dll"
            COMMENT "Copying wasmtime.dll to test output directory"
        )
    endif()

    # Add project root for tests
    target_compile_definitions(clasp_tests PRIVATE CLASP_PROJECT_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")

    enable_testing()
    add_test(NAME clasp_tests COMMAND clasp_tests)
endif()
