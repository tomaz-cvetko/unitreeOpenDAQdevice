set(LIB_NAME unitree_module)
set(MODULE_HEADERS_DIR ../include/${TARGET_FOLDER_NAME})

# Use Cyclone IDL compiler to generate source code
idlcxx_generate(TARGET cyclonedata FILES CycloneData.idl WARNINGS no-implicit-extensibility)

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
    cyclonedata
)

target_include_directories(${LIB_NAME} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
                                              $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/../include>
                                              $<INSTALL_INTERFACE:include>
)

opendaq_set_module_properties(${LIB_NAME} ${PROJECT_VERSION_MAJOR})
create_version_header(${LIB_NAME})
