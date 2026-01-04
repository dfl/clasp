# clasp-host: Development host plugin for WCLAPs
# Features: hot-reload, dev tools integration

# Sources
set(CLASP_HOST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/host_entry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/host_factory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/host_instance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/hot_reload.cpp
)

add_library(clasp-host MODULE ${CLASP_HOST_SOURCES})

target_include_directories(clasp-host PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link against clasp_core from parent project
target_link_libraries(clasp-host PRIVATE clasp_core)

# Set plugin output properties
set_target_properties(clasp-host PROPERTIES
    PREFIX ""
    SUFFIX ".clap"
    BUNDLE TRUE
    BUNDLE_EXTENSION "clap"
    MACOSX_BUNDLE_GUI_IDENTIFIER "com.clasp.dev-host"
    MACOSX_BUNDLE_BUNDLE_NAME "clasp-host"
    MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

if(APPLE)
    set_target_properties(clasp-host PROPERTIES
        MACOSX_BUNDLE TRUE
        XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES
    )
endif()
