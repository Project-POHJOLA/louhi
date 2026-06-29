set(PLUGIN_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src)
include_directories(${PLUGIN_INCLUDE_DIR})

set(CMAKE_AUTOMOC ON)

if(APPLE)
    set(PLUGIN_RPATH "@loader_path/../lib")
else()
    set(PLUGIN_RPATH "$ORIGIN/../lib")
endif()

macro(set_plugin_properties TARGET)
    set_target_properties(${TARGET} PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
        PREFIX ""
        INSTALL_RPATH "${PLUGIN_RPATH}"
        BUILD_RPATH_USE_ORIGIN ON
    )
endmacro()

add_library(natsplugin SHARED
    natsplugin.h
    natsplugin.cpp
    natssettingsdialog.h
    natssettingsdialog.cpp
    natsclient.cpp
    natsclient.h
)

target_link_libraries(natsplugin plugininterface Qt6::Widgets Qt6::Core Qt6::Gui nats_static)

set_plugin_properties(natsplugin)

add_library(messageviewerplugin SHARED
    messageviewerplugin.h
    messageviewerplugin.cpp
)

target_link_libraries(messageviewerplugin plugininterface Qt6::Widgets Qt6::Core Qt6::Gui)

set_plugin_properties(messageviewerplugin)

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

target_link_libraries(takplugin plugininterface Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network OpenSSL::Crypto)

set_plugin_properties(takplugin)

add_library(locationplugin SHARED
    locationplugin.h
    locationplugin.cpp
    locationsettingsdialog.h
    locationsettingsdialog.cpp
    gpsprovider.h
    gpsprovider.cpp
)

target_link_libraries(locationplugin plugininterface Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network Qt6::SerialPort)

if(Qt6Positioning_FOUND)
    target_link_libraries(locationplugin Qt6::Positioning)
    target_compile_definitions(locationplugin PRIVATE QT_POSITIONING_LIB)
endif()

set_plugin_properties(locationplugin)

add_library(mapplugin SHARED
    mapplugin.h
    mapplugin.cpp
    mapwidget.h
    mapwidget.cpp
)

target_link_libraries(mapplugin plugininterface Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network)

set_plugin_properties(mapplugin)

# ---------------------------------------------------------------------------
# OsgEarth Map Plugin (optional, requires osgEarth)
# ---------------------------------------------------------------------------
if(osgEarth_FOUND)
    add_library(osgearthplugin SHARED
        osgearthplugin.h
        osgearthplugin.cpp
        osgearthmapwidget.h
        osgearthmapwidget.cpp
        osgearthbasemapdock.h
        osgearthbasemapdock.cpp
        iconsetresolver.h
        iconsetresolver.cpp
    )

    target_link_libraries(osgearthplugin
        plugininterface
        Qt6::Widgets Qt6::Core Qt6::Gui Qt6::Network Qt6::OpenGLWidgets Qt6::Svg
        ${OSGEARTH_LIBRARIES}
    )

    target_include_directories(osgearthplugin PRIVATE
        ${OSGEARTH_INCLUDE_DIRS}
    )

    set_plugin_properties(osgearthplugin)

    message(STATUS "osgEarth found - building osgearthplugin")
else()
    message(STATUS "osgEarth not found - skipping osgearthplugin (install osgEarth to enable 3D map)")
endif()