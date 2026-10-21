# Self-contained bundle for running server_example on another PC (no Visual Studio, CycloneDDS or openDAQ install needed).
#
#   bin/      server_example.exe, dog_simulator.exe, unitree_module, the openDAQ native streaming server module,
#             and every DLL they load (openDAQ, ddsc, ddscxx, MSVC runtime). openDAQ loads modules from the
#             executable's folder, so everything sits next to the executables.
#   tools/    check_stream.py
#   README.md, start_server.bat
#
# Build the "bundle" target of a Release preset: out/build/<preset>/bundle/<name>/ and <name>.zip.
# Everything is installed under the install component "Bundle"; openDAQ's own install rules (headers,
# static libraries, ...) use other components and are left out.

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug binaries need the Visual Studio debug runtime, which is not redistributable
    add_custom_target(bundle
        COMMAND ${CMAKE_COMMAND} -E echo "The bundle must be built from a Release configuration (e.g. the x64-release preset)."
        COMMAND ${CMAKE_COMMAND} -E false
        VERBATIM
    )
    return()
endif()

set(BUNDLE_COMPONENT Bundle)
set(BUNDLE_NAME "UnitreeDog-${PROJECT_VERSION}-win64")
set(BUNDLE_ROOT "${CMAKE_BINARY_DIR}/bundle")
set(BUNDLE_DIR "${BUNDLE_ROOT}/${BUNDLE_NAME}")

# Targets the server needs at runtime: the executables, the plugin modules they load, and those modules' DLLs
set(_bundle_programs server_example dog_simulator)
set(_bundle_modules unitree_module native_stream_srv_module)

foreach (_target ${_bundle_programs})
    install(PROGRAMS $<TARGET_FILE:${_target}> DESTINATION bin COMPONENT ${BUNDLE_COMPONENT})
endforeach()
foreach (_target ${_bundle_modules})
    install(FILES $<TARGET_FILE:${_target}> DESTINATION bin COMPONENT ${BUNDLE_COMPONENT})
endforeach()
foreach (_target ${_bundle_programs} ${_bundle_modules})
    # Shared libraries the target links to (directly or indirectly), from this build and from CycloneDDS
    install(FILES $<TARGET_RUNTIME_DLLS:${_target}> DESTINATION bin COMPONENT ${BUNDLE_COMPONENT})
endforeach()

# MSVC runtime (vcruntime140.dll, msvcp140.dll, ...), so the target PC needs no Visual C++ Redistributable.
# Only the Release runtime may be redistributed: bundles are meant to be built from a Release preset.
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT ${BUNDLE_COMPONENT})
include(InstallRequiredSystemLibraries)

install(FILES "${PROJECT_SOURCE_DIR}/tools/check_stream.py" DESTINATION tools COMPONENT ${BUNDLE_COMPONENT})
install(FILES "${PROJECT_SOURCE_DIR}/bundle/README.md" "${PROJECT_SOURCE_DIR}/bundle/start_server.bat"
        DESTINATION . COMPONENT ${BUNDLE_COMPONENT})

file(MAKE_DIRECTORY "${BUNDLE_ROOT}")
add_custom_target(bundle
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${BUNDLE_DIR}" "${BUNDLE_DIR}.zip"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --config $<CONFIG> --component ${BUNDLE_COMPONENT} --prefix "${BUNDLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E tar cf "${BUNDLE_NAME}.zip" --format=zip "${BUNDLE_NAME}"
    COMMAND ${CMAKE_COMMAND} -E echo "Bundle: ${BUNDLE_DIR} (and ${BUNDLE_NAME}.zip)"
    WORKING_DIRECTORY "${BUNDLE_ROOT}"
    COMMENT "Creating the ${BUNDLE_NAME} bundle"
    VERBATIM
)
add_dependencies(bundle ${_bundle_programs} ${_bundle_modules})
