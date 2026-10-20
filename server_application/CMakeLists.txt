# ==== SERVER APPLICATION ===
# Creates the RobotDog device as root device and serves it over the openDAQ native protocol
add_executable(server_example main.cpp)

target_link_libraries(server_example
    PRIVATE
    daq::opendaq
)
# server_example loads the module and the native streaming server module at runtime, so build them with it
add_dependencies(
    unitree_module
    daq::native_stream_srv_module
)
# ===== ENDOF SERVER APPLICATION ===

# ===== DOG SIMULATOR =======
# Publishes fake LowState messages on rt/lowstate, for testing without the robot (see README)
add_executable(dog_simulator dog_simulator.cpp)
target_link_libraries(dog_simulator
    PRIVATE
    unitree_go_types
    $<$<PLATFORM_ID:Windows>:winmm>
)
# ===== ENDOF DOG SIMULATOR =======

# Put the matching-config runtime DLLs (opendaq, ddsc/ddscd, ddscxx/ddscxxd, ...) next to the executables
foreach(_exe server_example dog_simulator)
    add_custom_command(TARGET ${_exe} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${_exe}> $<TARGET_FILE_DIR:${_exe}>
        COMMAND_EXPAND_LISTS)
endforeach()
