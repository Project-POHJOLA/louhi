# PortableDeploy.cmake
#
# Adds a 'portable-deploy' custom target that bundles the application
# and all its shared library dependencies into a self-contained directory
# suitable for copying onto a USB stick.
#
# Enable with: cmake -DBUILD_PORTABLE=ON ..

# Determine Qt5 plugin directory using qmake
find_program(QT_QMAKE_EXECUTABLE qmake)
if(QT_QMAKE_EXECUTABLE)
    execute_process(
        COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
        OUTPUT_VARIABLE QT5_PLUGIN_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()
if(NOT EXISTS "${QT5_PLUGIN_DIR}")
    # Fallback: common locations
    foreach(_dir
        "/usr/lib/x86_64-linux-gnu/qt5/plugins"
        "/usr/lib/qt5/plugins"
        "/usr/local/opt/qt/lib/plugins"
    )
        if(EXISTS "${_dir}")
            set(QT5_PLUGIN_DIR "${_dir}")
            break()
        endif()
    endforeach()
endif()

message(STATUS "PortableDeploy: Qt5 install prefix = ${QT5_INSTALL_PREFIX}")
message(STATUS "PortableDeploy: Qt5 plugin dir     = ${QT5_PLUGIN_DIR}")

set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/Louhi.app")
set(DEPLOY_BIN_DIR "${DEPLOY_DIR}/bin")
set(DEPLOY_LIB_DIR "${DEPLOY_DIR}/lib")
set(DEPLOY_PLUGIN_DIR "${DEPLOY_DIR}/plugins")

# Collect all our binaries for ldd analysis
set(OUR_BINARIES
    "$<TARGET_FILE:louhi>"
    "$<TARGET_FILE:plugininterface>"
    "$<TARGET_FILE:natsplugin>"
    "$<TARGET_FILE:messageviewerplugin>"
    "$<TARGET_FILE:takplugin>"
    "$<TARGET_FILE:locationplugin>"
    "$<TARGET_FILE:mapplugin>"
    "$<TARGET_FILE:osgearthplugin>"
)

# Build a list of deploy sub-targets we depend on
set(DEPLOY_DEPS
    louhi
    plugininterface
    natsplugin
    messageviewerplugin
    takplugin
    locationplugin
    mapplugin
    osgearthplugin
)

# Generate the deploy script
set(DEPLOY_SCRIPT "${CMAKE_BINARY_DIR}/portable-deploy.sh")
configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/portable-deploy.sh.in"
    "${DEPLOY_SCRIPT}"
    @ONLY
)

add_custom_target(portable-deploy
    COMMAND bash "${DEPLOY_SCRIPT}"
    COMMENT "Building portable deployment bundle at ${DEPLOY_DIR}"
    DEPENDS ${DEPLOY_DEPS}
)
