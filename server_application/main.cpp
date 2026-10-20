/**
 * Server application
 */

#include <iostream>
#include <opendaq/opendaq.h>

using namespace daq;

int main(int /*argc*/, const char* /*argv*/[])
{
    std::cout << ":)" << std::endl;

    // Modules (unitree_module, native streaming server) are loaded from this executable's folder
    const auto instance = InstanceBuilder()
                            .setRootDevice("daq.dog://DewesoftRobotics_B42D4000O4N9D402")
                            .addDiscoveryServer("mdns")
                            .build();
    instance.addServer("OpenDAQNativeStreaming", nullptr).enableDiscovery();
    std::cout << "Press \"enter\" to exit the application..." << std::endl;
    std::cin.get();
    return 0;
}
