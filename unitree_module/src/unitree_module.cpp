#include <coretypes/version_info_factory.h>
#include <opendaq/custom_log.h>
#include <unitree_module/unitree_module.h>
#include <unitree_module/unitree_device.h>
#include <unitree_module/version.h>
// #include <unitree_module/test_device_impl.h>
#include <opendaq/device_type_factory.h>
#include <iostream>

BEGIN_NAMESPACE_UNITREE_MODULE

UnitreeModule::UnitreeModule(ContextPtr context)
    : Module("ReferenceDeviceModule",
             VersionInfo(UNITREE_MODULE_MAJOR_VERSION, UNITREE_MODULE_MINOR_VERSION, UNITREE_MODULE_PATCH_VERSION),
             std::move(context),
             UNITREE_MODULE_NAME)
    , deviceAdded(false)
    , deviceIndex(0)
{
}

ListPtr<IDeviceInfo> UnitreeModule::onGetAvailableDevices()
{
    // TODO: Different modules handle this differently - potentially one device should be created immediately and
    // always available?

    /*ListPtr<IDeviceInfo> availableDevices = List<IDeviceInfo>();
    std::cout << "Hello from UnitreeModule::onGetAvailableDevices()" << std::endl;

    availableDevices.pushBack(UnitreeDevice::CreateDeviceInfo());

    return availableDevices;*/

    return {UnitreeDevice::CreateDeviceInfo()};
}

DictPtr<IString, IDeviceType> UnitreeModule::onGetAvailableDeviceTypes()
{
    auto type = UnitreeDevice::CreateType();
    return Dict<IString, IDeviceType>({{type.getId(), type}});
}

DevicePtr UnitreeModule::onCreateDevice(const StringPtr& connectionString,
                                        const ComponentPtr& parent,
                                        const PropertyObjectPtr& /*config*/)
{
    std::scoped_lock lock(sync);

    std::string connStr = connectionString;
    if (connStr.find("daq.dog://") != 0)
        throw std::runtime_error("Invalid connection string prefix");

    if (deviceAdded)
        throw std::runtime_error("Only one device can be created");

    auto devicePtr = createWithImplementation<IDevice, UnitreeDevice>(context, parent);

    deviceAdded = true;
    return devicePtr;
}

END_NAMESPACE_UNITREE_MODULE
