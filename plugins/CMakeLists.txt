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

target_link_libraries(natsplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui nats_static)

set_target_properties(natsplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH_USE_ORIGIN ON
)

add_library(messageviewerplugin SHARED
    messageviewerplugin.h
    messageviewerplugin.cpp
)

target_link_libraries(messageviewerplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui)

set_target_properties(messageviewerplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH_USE_ORIGIN ON
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
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH_USE_ORIGIN ON
)

add_library(locationplugin SHARED
    locationplugin.h
    locationplugin.cpp
    locationsettingsdialog.h
    locationsettingsdialog.cpp
    gpsprovider.h
    gpsprovider.cpp
)

target_link_libraries(locationplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Network)

set_target_properties(locationplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH_USE_ORIGIN ON
)

add_library(mapplugin SHARED
    mapplugin.h
    mapplugin.cpp
    mapwidget.h
    mapwidget.cpp
)

target_link_libraries(mapplugin plugininterface Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Network)

set_target_properties(mapplugin PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
    PREFIX ""
    INSTALL_RPATH "$ORIGIN/../lib"
    BUILD_RPATH_USE_ORIGIN ON
)

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
    )

    target_link_libraries(osgearthplugin
        plugininterface
        Qt5::Widgets Qt5::Core Qt5::Gui Qt5::Network
        ${OSGEARTH_LIBRARIES}
    )

    target_include_directories(osgearthplugin PRIVATE
        ${OSGEARTH_INCLUDE_DIRS}
    )

    set_target_properties(osgearthplugin PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugins
        PREFIX ""
        INSTALL_RPATH "$ORIGIN/../lib"
        BUILD_RPATH_USE_ORIGIN ON
    )

    message(STATUS "osgEarth found - building osgearthplugin")
else()
    message(STATUS "osgEarth not found - skipping osgearthplugin (install osgEarth to enable 3D map)")
endif()