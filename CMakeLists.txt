cmake_minimum_required(VERSION 3.16)
project(Louhi VERSION 0.1.0 LANGUAGES CXX)

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
find_library(NATS_LIBRARY nats)

include_directories(${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})

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

target_link_libraries(louhi plugininterface Qt5::Widgets Qt5::Gui ${NATS_LIBRARY})

add_subdirectory(plugins)

set(PLUGIN_DIR ${CMAKE_BINARY_DIR}/plugins)
file(MAKE_DIRECTORY ${PLUGIN_DIR})

add_custom_target(copy_plugins ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/plugins ${PLUGIN_DIR}
    COMMENT "Copying plugins directory"
)

install(TARGETS louhi plugininterface DESTINATION bin)
install(DIRECTORY plugins/ DESTINATION plugins FILES_MATCHING PATTERN "*.json")