set(PLUGIN_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src)
include_directories(${PLUGIN_INCLUDE_DIR})

set(CMAKE_AUTOMOC ON)

add_library(natsplugin SHARED
    natsplugin.h
    natsplugin.cpp
    natssettingsdialog.h
    natssettingsdialog.cpp
    natsclient.cpp
    natsclient.h
)

target_link_libraries(natsplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui ${NATS_LIBRARY})

set_target_properties(natsplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
)

add_library(messageviewerplugin SHARED
    messageviewerplugin.h
    messageviewerplugin.cpp
)

target_link_libraries(messageviewerplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui)

set_target_properties(messageviewerplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
)

add_library(takplugin SHARED
    takplugin.h
    takplugin.cpp
    takserverconnection.h
    takserverconnection.cpp
    taksettingsdialog.h
    taksettingsdialog.cpp
    cotmessage.h
    cotmessage.cpp
)

target_link_libraries(takplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Network Qt5::Xml OpenSSL::Crypto)

set_target_properties(takplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
)