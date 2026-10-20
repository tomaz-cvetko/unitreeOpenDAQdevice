add_library(unitree_dds INTERFACE)
# target_link_libraries(unitree_dds INTERFACE
#     D:/Razno/unitree_go2/unitreeDDS/src/ddscxx/include
# )

# ==== SERVER APPLICATION ===
# Instruct the build system to make a new program ("EXE" file)
add_executable(server_example main.cpp)

# Connect the "EXE" file with libraries it uses
target_link_libraries(server_example
    PRIVATE
    daq::opendaq
)
# Leftover from opendaq example_module
add_dependencies(
    unitree_module
    daq::native_stream_srv_module
)
# ===== ENDOF SERVER APPLICATION ===

# ===== CYCLONE EXAMPLE APP =======
# Use Cyclone IDL compiler to generate source code
# idlcxx_generate(TARGET helloworlddata FILES HelloWorldData.idl WARNINGS no-implicit-extensibility)

# Instruct the build system to make a new program ("EXE" file)
add_executable(cyclone_example cyclone_main.cpp)

# target_include_directories(cyclone_example
#   PRIVATE
#   unitree_dds
# )

# Connect the "EXE" file with libraries it uses
target_link_libraries(cyclone_example
    PRIVATE
    daq::opendaq
    CycloneDDS-CXX::ddscxx
    # helloworlddata
)

add_executable(test_main test_main.cpp)
target_link_libraries(test_main
    PRIVATE
    daq::opendaq
    CycloneDDS-CXX::ddscxx
    # helloworlddata
)

# if (TARGET helloworlddata)
#     message(STATUS "Target helloworlddata exists")
# else()
#     message(FATAL_ERROR "Target helloworlddata DOES NOT exist")
# endif()

get_target_property(SRCS test_main SOURCES)
message(STATUS "test_main sources: ${SRCS}")

# Put the matching-config runtime DLLs (opendaq, ddsc/ddscd, ddscxx/ddscxxd, ...) next to the executables
foreach(_exe server_example cyclone_example test_main)
    add_custom_command(TARGET ${_exe} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${_exe}> $<TARGET_FILE_DIR:${_exe}>
        COMMAND_EXPAND_LISTS)
endforeach()
# ==== ENDOF CYCLONE EXAMPLE APP ===