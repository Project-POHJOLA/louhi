# FindosgEarth.cmake
# Finds the osgEarth libraries and header files
#
# This module sets:
#   OSGEARTH_FOUND          - True if osgEarth was found
#   OSGEARTH_INCLUDE_DIRS   - Include directories
#   OSGEARTH_LIBRARIES      - Libraries to link against
#   OSGEARTH_VERSION        - Version string

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_OSGEARTH QUIET osgEarth)
endif()

find_path(OSGEARTH_INCLUDE_DIR
    NAMES osgEarth/MapNode
    PATHS
        ${PC_OSGEARTH_INCLUDE_DIRS}
        /usr/include
        /usr/local/include
        /opt/local/include
        /sw/include
    PATH_SUFFIXES osgEarth
)

find_library(OSGEARTH_LIBRARY
    NAMES osgEarth
    PATHS
        ${PC_OSGEARTH_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        /sw/lib
)

find_library(OSGEARTH_UTIL_LIBRARY
    NAMES osgEarthUtil
    PATHS
        ${PC_OSGEARTH_LIBRARY_DIRS}
        /usr/lib
        /usr/local/lib
        /opt/local/lib
        /sw/lib
)

set(OSGEARTH_INCLUDE_DIRS ${OSGEARTH_INCLUDE_DIR})
set(OSGEARTH_LIBRARIES ${OSGEARTH_LIBRARY})
if(OSGEARTH_UTIL_LIBRARY)
    list(APPEND OSGEARTH_LIBRARIES ${OSGEARTH_UTIL_LIBRARY})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(osgEarth
    FOUND_VAR OSGEARTH_FOUND
    REQUIRED_VARS OSGEARTH_INCLUDE_DIR OSGEARTH_LIBRARY
)

mark_as_advanced(
    OSGEARTH_INCLUDE_DIR
    OSGEARTH_LIBRARY
    OSGEARTH_UTIL_LIBRARY
)
