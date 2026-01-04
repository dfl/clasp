cmake_minimum_required(VERSION 3.21)

# Read version from VERSION file
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" CLASP_VERSION_FULL)
string(STRIP "${CLASP_VERSION_FULL}" CLASP_VERSION_FULL)
# Extract numeric version (before any -alpha, -beta, etc.)
string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" CLASP_VERSION "${CLASP_VERSION_FULL}")

project(clasp VERSION ${CLASP_VERSION} LANGUAGES CXX C)
message(STATUS "CLASP version: ${CLASP_VERSION_FULL}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Options
option(CLASP_BUILD_EXAMPLES "Build example WASM plugins" ON)

# Include our cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Fetch Wasmtime
include(FetchWasmtime)

# Add external dependencies as submodules
add_subdirectory(extern/clap EXCLUDE_FROM_ALL)
add_subdirectory(extern/clap-helpers EXCLUDE_FROM_ALL)

# CHOC is header-only, just need to add include path
set(CHOC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/extern/choc")

# clasp library (shared code)
set(CLASP_CORE_SOURCES
    src/runtime.cpp
    src/scanner.cpp
    src/params.cpp
    src/state.cpp
    src/gui.cpp
)

# Add platform-specific GUI embedding
if(APPLE)
    list(APPEND CLASP_CORE_SOURCES src/gui_cocoa.mm)
    set_source_files_properties(src/gui_cocoa.mm PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
elseif(WIN32)
    list(APPEND CLASP_CORE_SOURCES src/gui_win32.cpp)
elseif(UNIX AND NOT APPLE)
    list(APPEND CLASP_CORE_SOURCES src/gui_linux.cpp)
endif()

add_library(clasp_core STATIC ${CLASP_CORE_SOURCES})

target_include_directories(clasp_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CHOC_INCLUDE_DIR}
    ${WASMTIME_INCLUDE_DIR}
)

target_link_libraries(clasp_core PUBLIC
    clap
    clap-helpers
    ${WASMTIME_LIBRARIES}
)

# Platform-specific libraries for CHOC WebView
if(APPLE)
    find_library(WEBKIT_FRAMEWORK WebKit REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    target_link_libraries(clasp_core PUBLIC
        ${WEBKIT_FRAMEWORK}
        ${COCOA_FRAMEWORK}
    )
elseif(WIN32)
    # WebView2 will be loaded dynamically by CHOC
elseif(UNIX)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.0)
    target_include_directories(clasp_core PUBLIC ${WEBKIT2_INCLUDE_DIRS})
    target_link_libraries(clasp_core PUBLIC ${WEBKIT2_LIBRARIES})
endif()

# clasp.clap plugin
add_library(clasp MODULE
    src/plugin_entry.cpp
    src/plugin_factory.cpp
    src/plugin_instance.cpp
)

target_link_libraries(clasp PRIVATE clasp_core)

# Set plugin output properties
set_target_properties(clasp PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    BUNDLE TRUE
    BUNDLE_EXTENSION "clap"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.clasp.wrapper"
    MACOSX_BUNDLE_BUNDLE_NAME "clasp"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

# macOS: Build as proper bundle
if(APPLE)
    set_target_properties(clasp PROPERTIES
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

install(TARGETS clasp
    LIBRARY DESTINATION "${CLAP_INSTALL_DIR}"
    BUNDLE DESTINATION "${CLAP_INSTALL_DIR}"
)

# CLI Tools
add_executable(clasp-precompile tools/precompile.cpp)
target_link_libraries(clasp-precompile PRIVATE clasp_core)
# CLI tool is not installed - run from build directory: ./build/clasp-precompile

# Examples (WASM plugins are built separately with wasi-sdk/cargo)
if(CLASP_BUILD_EXAMPLES)
    # Copy example plugin.json files to build directory for testing
    file(GLOB EXAMPLE_MANIFESTS "examples/*/plugin.json")
    foreach(MANIFEST ${EXAMPLE_MANIFESTS})
        get_filename_component(EXAMPLE_DIR ${MANIFEST} DIRECTORY)
        get_filename_component(EXAMPLE_NAME ${EXAMPLE_DIR} NAME)
        configure_file(${MANIFEST} ${CMAKE_BINARY_DIR}/examples/${EXAMPLE_NAME}.clasp/plugin.json COPYONLY)
    endforeach()
endif()
