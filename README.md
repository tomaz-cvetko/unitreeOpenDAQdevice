# Example device module

Simple example that builds an openDAQ module giving access to an example device. Said device contains 2 channels, each with a value and time signal. The value signal outputs a counter signal, increasing at a rate of 1 per second.

## Testing the module

To test the module, enable the `OPENDAQ_DEVICE_EXAMPLE_ENABLE_SERVER_APP` cmake flag. Doing so will add an openDAQ native server module to your build targets, and add an additional "server_application" executable. Running the executable will create a device, add an openDAQ server, and enable discovery.

To connect to the device, the openDAQ Python GUI application can be used (Latest Python version is recommended):

```
py -m pip install openDAQ
py -m openDAQ
```
Instructions:
- [OPTIONAL] Go to https://docs-dev.opendaq.com/ , download the correct installer and change the version in external/openDAQ/CMakeLists.txt to match what you installed. This will prevent downloading opendaq too many times.
- Build and install CycloneDDS (e.g. to C:/Program Files/CycloneDDS/) as instructed here https://cyclonedds.io/docs/cyclonedds/latest/installation/installation.html (Core DDS C)
- Build and install CycloneDDS-Cxx (e.g. to C:/Program Files/CycloneDDS-Cxx/) as instructed here https://cyclonedds.io/docs/cyclonedds/latest/installation/installation.html (C++)
- Note: if installing to Program Files, the terminal will need to be run as administrator
- Add both CycloneDDS/bin and CycloneDDS-Cxx/bin to Path, so that .dll-s can be found.
- CMakeLists.txt uses find_package to find these two libraries.
- Now the cyclone tools are available in this project, you can use the pattern to generate IDL files and use them in your code:
```
idlcxx_generate(TARGET <idlc-target> ...)
add_executable(<executable> ...)
target_link_libraries(<executable> CycloneDDS-CXX:ddscxx <idlc-target>)

add_library(<lib> ...)
target_link_libraries(<lib> CycloneDDS-CXX::ddscxx <idlc-target>)
```
- Build the project with `OPENDAQ_DEVICE_EXAMPLE_ENABLE_SERVER_APP=ON` and then try running `cyclone_example.exe` (should write two lines and not exit untill interrupted). Exiting without output likely means missing DLLs.

## Useful links

- https://support.unitree.com/home/en/developer/Basic_services (explanation of LowState and MotorState data)
- https://support.unitree.com/home/en/developer/sports_services (explanation of SportModeState and IMUState data)