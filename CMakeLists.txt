cmake_minimum_required(VERSION 3.25)
set(REPO_NAME example_device_module)
set(REPO_OPTION_PREFIX EXAMPLE_MODULE)

project(${REPO_NAME} VERSION 1.0.0)

# ==== CycloneDDS (see README, "CycloneDDS setup") ====
# One install prefix holding Debug (ddscd, ddscxxd) and Release (ddsc, ddscxx) builds, made by tools/setup_cyclonedds.ps1.
set(CYCLONEDDS_REQUIRED_VERSION 11.0.1)
if (POLICY CMP0144)
    cmake_policy(SET CMP0144 NEW) # CYCLONEDDS_ROOT is also honoured as a find_package search root
endif()
set(CYCLONEDDS_ROOT "$ENV{CYCLONEDDS_ROOT}" CACHE PATH "Install prefix of CycloneDDS + CycloneDDS-CXX ${CYCLONEDDS_REQUIRED_VERSION} (see README)")
if (CYCLONEDDS_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${CYCLONEDDS_ROOT}")
endif()

# Optimized configs must use the Release Cyclone libs, never the Debug ones.
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Release)

find_package(CycloneDDS ${CYCLONEDDS_REQUIRED_VERSION} EXACT QUIET)
find_package(CycloneDDS-CXX ${CYCLONEDDS_REQUIRED_VERSION} EXACT QUIET)
if (NOT CycloneDDS_FOUND OR NOT CycloneDDS-CXX_FOUND)
    message(FATAL_ERROR
        "CycloneDDS ${CYCLONEDDS_REQUIRED_VERSION} (C and C++) not found (CYCLONEDDS_ROOT='${CYCLONEDDS_ROOT}').\n"
        "Run tools/setup_cyclonedds.ps1 and set the CYCLONEDDS_ROOT environment variable to its install prefix "
        "(restart Visual Studio afterwards), or set CYCLONEDDS_ROOT in CMakeUserPresets.json. See README.md.")
endif()
message(STATUS "CycloneDDS ${CycloneDDS_VERSION} config: ${CycloneDDS_DIR}")
message(STATUS "CycloneDDS-CXX config: ${CycloneDDS-CXX_DIR}")

# MSVC cannot mix Debug and Release C++ runtimes, so the Cyclone install must provide the config being built.
get_target_property(_cyclone_configs CycloneDDS-CXX::ddscxx IMPORTED_CONFIGURATIONS)
if (CMAKE_CONFIGURATION_TYPES)
    set(_needed_configs DEBUG RELEASE)
elseif (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(_needed_configs DEBUG)
else()
    set(_needed_configs RELEASE)
endif()
foreach (_cfg ${_needed_configs})
    if (NOT _cfg IN_LIST _cyclone_configs)
        message(FATAL_ERROR
            "The CycloneDDS install at '${CycloneDDS-CXX_DIR}' has no ${_cfg} libraries (it has: ${_cyclone_configs}).\n"
            "Install both Debug and Release into the same prefix with tools/setup_cyclonedds.ps1. See README.md.")
    endif()
endforeach()

# Code generation always runs the Release idlc and C++ backend, also in Debug builds
# (a Debug-CRT backend loaded into a Release idlc.exe is not safe).
foreach (_t CycloneDDS::idlc CycloneDDS::idl CycloneDDS-CXX::idlcxx)
    if (TARGET ${_t})
        set_target_properties(${_t} PROPERTIES MAP_IMPORTED_CONFIG_DEBUG "Release;")
    endif()
endforeach()

if (POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

if (POLICY CMP0077)
    cmake_policy(SET CMP0077 NEW)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MESSAGE_CONTEXT ${REPO_NAME})
set(CMAKE_MESSAGE_CONTEXT_SHOW ON CACHE BOOL "Show CMake message context")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

add_definitions(-DFMT_HEADER_ONLY)

option(OPENDAQ_DEVICE_EXAMPLE_ENABLE_SERVER_APP "Enable building example server application" OFF)

include(CommonUtils)
setup_repo(${REPO_OPTION_PREFIX})

if(OPENDAQ_DEVICE_EXAMPLE_ENABLE_SERVER_APP)
	set(DAQMODULES_OPENDAQ_SERVER_MODULE ON CACHE BOOL "" FORCE)
	set(OPENDAQ_ENABLE_NATIVE_STREAMING ON CACHE BOOL "" FORCE)
endif()

add_subdirectory(external)
add_subdirectory(unitree_module) # Means: go execute unitree_module/CMakeLists.txt

if(OPENDAQ_DEVICE_EXAMPLE_ENABLE_SERVER_APP)
    message(STATUS "Enabled example server application")
    add_subdirectory(server_application)

    # "bundle" target: a folder/zip with everything needed to run server_example on another PC
    if (WIN32)
        include(Bundle)
    endif()
endif()
