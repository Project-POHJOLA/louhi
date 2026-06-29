# PortableDeploy.cmake
#
# Adds a 'portable-deploy' custom target that bundles the application
# and all its shared library dependencies into a self-contained directory.
#
# Platform support:
#   Linux   - Uses ldd + custom bash script + patchelf/chrpath for RPATH
#   macOS   - Uses macdeployqt (Qt-provided tool)
#   Windows - Uses windeployqt (Qt-provided tool)
#
# Enable with: cmake -DBUILD_PORTABLE=ON ..

if(APPLE)
    # ------------------------------------------------------------------
    # macOS: use macdeployqt
    # ------------------------------------------------------------------
    find_program(MACDEPLOYQT macdeployqt)
    if(NOT MACDEPLOYQT)
        message(FATAL_ERROR "macdeployqt not found. Install Qt6 development tools.")
    endif()

    add_custom_target(portable-deploy
        COMMAND ${MACDEPLOYQT} "$<TARGET_FILE_DIR:louhi>/../.."
            -executable="$<TARGET_FILE:louhi>"
            -verbose=1
        COMMENT "Building macOS application bundle with macdeployqt"
        DEPENDS louhi natsplugin messageviewerplugin takplugin locationplugin mapplugin
    )

elseif(WIN32)
    # ------------------------------------------------------------------
    # Windows: use windeployqt
    # ------------------------------------------------------------------
    find_program(WINDEPLOYQT windeployqt)
    if(NOT WINDEPLOYQT)
        message(FATAL_ERROR "windeployqt not found. Install Qt6 development tools.")
    endif()

    set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/Louhi")
    set(DEPLOY_PLUGIN_DIR "${DEPLOY_DIR}/plugins")

    add_custom_target(portable-deploy
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_PLUGIN_DIR}"
        COMMAND ${WINDEPLOYQT} "$<TARGET_FILE:louhi>" --dir "${DEPLOY_DIR}"
            --plugindir "${DEPLOY_PLUGIN_DIR}" --release
        COMMENT "Building Windows deployment bundle with windeployqt"
        DEPENDS louhi natsplugin messageviewerplugin takplugin locationplugin mapplugin
    )

else()
    # ------------------------------------------------------------------
    # Linux: use ldd + custom bash script + patchelf/chrpath
    # ------------------------------------------------------------------

    # Determine Qt6 plugin directory using qmake6
    find_program(QT_QMAKE_EXECUTABLE qmake6)
    if(QT_QMAKE_EXECUTABLE)
        execute_process(
            COMMAND ${QT_QMAKE_EXECUTABLE} -query QT_INSTALL_PLUGINS
            OUTPUT_VARIABLE QT6_PLUGIN_DIR
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
    endif()
    if(NOT EXISTS "${QT6_PLUGIN_DIR}")
        # Fallback: common locations
        foreach(_dir
            "/usr/lib/x86_64-linux-gnu/qt6/plugins"
            "/usr/lib/qt6/plugins"
        )
            if(EXISTS "${_dir}")
                set(QT6_PLUGIN_DIR "${_dir}")
                break()
            endif()
        endforeach()
    endif()

    message(STATUS "PortableDeploy: Qt6 plugin dir     = ${QT6_PLUGIN_DIR}")

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
endif()
