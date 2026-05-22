cmake_minimum_required(VERSION 3.16)
project(Louhi VERSION 0.3.0 LANGUAGES CXX)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_BINARY_DIR}/version.h
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Core Gui Network Xml)
find_package(OpenSSL REQUIRED)

# ---------------------------------------------------------------------------
# NATS C Client (git submodule, built statically)
# ---------------------------------------------------------------------------
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_LIB_SHARED OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_LIB_STATIC ON CACHE BOOL "" FORCE)
set(NATS_BUILD_STREAMING OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_USE_SODIUM OFF CACHE BOOL "" FORCE)
set(NATS_BUILD_WITH_TLS ON CACHE BOOL "" FORCE)
# Static lib must be compiled with PIC since it links into .so plugins
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_subdirectory(deps/nats.c EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# Enable building portable deployment bundle
# ---------------------------------------------------------------------------
option(BUILD_PORTABLE "Build portable deployment bundle" OFF)

if(BUILD_PORTABLE)
    include(cmake/PortableDeploy.cmake)
endif()

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# Plugin interface library
# ---------------------------------------------------------------------------
add_library(plugininterface SHARED
    src/plugininterface.h
    src/pluginmanager.h
    src/configmanager.h
    src/natsmessage.h
    src/pluginmanager.cpp
    src/configmanager.cpp
)

target_link_libraries(plugininterface Qt5::Core Qt5::Widgets)

set_target_properties(plugininterface PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)

# ---------------------------------------------------------------------------
# Main executable
# ---------------------------------------------------------------------------
add_executable(louhi
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/pluginmanagerdialog.cpp
    src/pluginmanagerdialog.h
    src/connectionstatusled.cpp
    src/connectionstatusled.h
    src/connectionledmanager.cpp
    src/connectionledmanager.h
    resources.qrc
)

target_link_libraries(louhi plugininterface Qt5::Widgets Qt5::Gui)

# Set RPATH for portable deployment: look for libs in ../lib/ relative to binary (bin/ -> lib/)
set_target_properties(louhi PROPERTIES
    INSTALL_RPATH "$ORIGIN/../lib"
)

# ---------------------------------------------------------------------------
# Plugins
# ---------------------------------------------------------------------------
add_subdirectory(plugins)

set(PLUGIN_DIR ${CMAKE_BINARY_DIR}/plugins)
file(MAKE_DIRECTORY ${PLUGIN_DIR})

add_custom_target(copy_plugins ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/plugins ${PLUGIN_DIR}
    COMMENT "Copying plugins directory"
)

# ---------------------------------------------------------------------------
# Installation rules (used by portable deploy target)
# ---------------------------------------------------------------------------
install(TARGETS louhi DESTINATION bin)
install(TARGETS plugininterface DESTINATION lib)
install(TARGETS natsplugin messageviewerplugin takplugin locationplugin mapplugin
    LIBRARY DESTINATION plugins
)
install(DIRECTORY plugins/ DESTINATION plugins FILES_MATCHING PATTERN "*.json")