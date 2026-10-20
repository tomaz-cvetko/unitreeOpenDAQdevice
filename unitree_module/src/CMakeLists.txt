set(LIB_NAME unitree_module)
set(MODULE_HEADERS_DIR ../include/${TARGET_FOLDER_NAME})

set(SRC_Include common.h
                module_dll.h
                unitree_module.h
                unitree_device.h
                unitree_channel.h
                sensor_data_listener.h
)

set(SRC_Srcs module_dll.cpp
             unitree_module.cpp
             unitree_device.cpp
             unitree_channel.cpp
             sensor_data_listener.cpp
)

prepend_include(${TARGET_FOLDER_NAME} SRC_Include)

source_group("module" FILES ${MODULE_HEADERS_DIR}/unitree_module.h
                            ${MODULE_HEADERS_DIR}/unitree_channel.h
                            ${MODULE_HEADERS_DIR}/unitree_device.h
                            ${MODULE_HEADERS_DIR}/module_dll.h
                            module_dll.cpp
                            unitree_module.cpp
                            unitree_device.cpp
                            unitree_channel.cpp
)


# Unitree Go2 DDS message types (generated code), shared by the module and the dog_simulator tool
add_library(unitree_go_types STATIC IMUState.cpp
                                    MotorState.cpp
                                    BmsState.cpp
                                    LowState.cpp
)
target_include_directories(unitree_go_types PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include
                                                   ${CMAKE_CURRENT_SOURCE_DIR}/../include/unitree_module
)
target_link_libraries(unitree_go_types PUBLIC CycloneDDS-CXX::ddscxx)

add_library(${LIB_NAME} SHARED ${SRC_Include}
                               ${SRC_Srcs}
)

add_library(${SDK_TARGET_NAMESPACE}::${LIB_NAME} ALIAS ${LIB_NAME})

if (MSVC)
    target_compile_options(${LIB_NAME} PRIVATE /bigobj)
endif()

target_link_libraries(${LIB_NAME}
    PUBLIC
    daq::opendaq
    CycloneDDS-CXX::ddscxx
    unitree_go_types
)

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include/unitree_module>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
                                              $<INSTALL_INTERFACE:include>
)

opendaq_set_module_properties(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
create_version_header(${LIB_NAME})

# The module is loaded as a plugin at runtime, so its own dependencies (ddsc[d].dll, ddscxx[d].dll)
# must be copied next to it; the executables' runtime-DLL lists do not include them.
if (WIN32)
    add_custom_command(TARGET ${LIB_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${LIB_NAME}> $<TARGET_FILE_DIR:${LIB_NAME}>
        COMMAND_EXPAND_LISTS)
endif()
