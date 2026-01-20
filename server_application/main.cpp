/**
 * Server application
 */

/*
 * TODO:
 * - Copy files to existing ExampleModule -> DONE
 * - Rename files to properly represent what the module is doing -> DONE
 * - Fix CMake to find the files -> DONE
 * - Fix include paths to the new file names -> DONE
 * - Swap myDDSDevice class with Subscriber; call "function" in data callback of subscriber
 * - Provide vector of data through the callback arguments;
 *   - The argument type should be vector<uint16_t> instead of uint16_t
 * - Update signal names to properly represent their data -> DONE
 * - Set value signal packet data to the ones obtained from subscriber
 * - Create `main` function that
 *   - Sets the device as root,
 *   - Adds "mdns" discovery server,
 *   - Adds the "OpenDAQNativeStreaming" server
 *   - Enables discovery on said server
 */

#include <iostream>
#include <opendaq/opendaq.h>

using namespace daq;

int main(int /*argc*/, const char* /*argv*/[])
{
    std::cout << ":)" << std::endl;

    // Create an Instance, loading modules at MODULE_PATH
    const auto instance = InstanceBuilder()
                              .addModulePath("C:/Users/tomaz/work/anamahau/unitreeOpenDAQdevice/out/build/x64/msvc-22/debug/bin/Debug")
                              .setRootDevice("daq.dog://DewesoftRobotics_B42D4000O4N9D402")
                              .addDiscoveryServer("mdns")
                              .build();
    instance.addServer("OpenDAQNativeStreaming", nullptr).enableDiscovery();

    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
