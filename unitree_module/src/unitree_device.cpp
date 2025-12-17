#include <coreobjects/unit_factory.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/device_domain_factory.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/sync_component_private_ptr.h>
#include <opendaq/thread_name.h>

#include <unitree_module/sensor_data_listener.h>
#include <unitree_module/unitree_channel.h>
#include <unitree_module/unitree_device.h>

#include <chrono>
#include <iostream>

// #include<LowState.hpp>

//#include<helloworld_unitree/subscriber.cpp>

//#include<>

// #include <dds/dds.hpp>
// #include <dds/domain/qos/DomainParticipantQos.hpp>
// #include <LowState.hpp>

/*
Additional Include Directories
D:\Razno\unitree_go2\cyclonedds-cxx-master\src\ddscxx\include
D:\Razno\unitree_go2\cyclonedds-cxx-master\build\src\ddscxx\include
D:\Razno\unitree_go2\cyclonedds-cxx-master\build\examples\helloworld
D:\Razno\unitree_go2\cyclonedds-master\cyclonedds-master\install\include
D:\Razno\unitree_go2\cyclonedds-master\cyclonedds-master\install\include\ddscxx

Windows PowerShell
Get-ChildItem -Path D:\Razno\unitree_go2 -Filter CycloneDDSConfig.cmake -Recurse
*/

BEGIN_NAMESPACE_UNITREE_MODULE

namespace
{
    constexpr bool CallbackQueueing = false;
}

UnitreeDevice::UnitreeDevice(const ContextPtr& ctx, const ComponentPtr& parent)
    : GenericDevice<>(ctx, parent, "DewesoftRobotics_B42D4000O4N9D402", nullptr, "RobotDog")
    , microSecondsFromEpochToDeviceStart(0)
    , stopAcq(false)
    , acqThread([this]() { this->acqLoop(); })
{
    this->loggerComponent = ctx.getLogger().getOrAddComponent(UNITREE_MODULE_NAME);
    initDomain();
    initChannels();

    /****************************************/
    // Unitree DDS
    dds::domain::DomainParticipant participant(0);
    dds::topic::Topic<SensorDataListener::MsgType> topic(participant, SensorDataListener::GetTopicName());
    dds::sub::Subscriber subscriber(participant);

    dds::sub::DataReader<SensorDataListener::MsgType> reader(
        subscriber,
        topic,
        subscriber.default_datareader_qos(),
        new SensorDataListener([this](std::vector<int16_t>&& data) { this->forwardDataCallback(std::move(data)); }),
        dds::core::status::StatusMask::data_available());
    std::cout << "Listening for Unitree Go2 sensor data on topic '" << SensorDataListener::GetTopicName() << "'..." << std::endl;
    /****************************************/
}

UnitreeDevice::~UnitreeDevice()
{
    // TODO: Proper shutdown
    {
        auto lock = this->getAcquisitionLock();
        stopAcq = true;
    }

    acqThread.join();
}

DeviceInfoPtr UnitreeDevice::CreateDeviceInfo()
{
    // auto devInfo = DeviceInfo("example://device");
    auto devInfo = DeviceInfo("daq.dog://DewesoftRobotics_B42D4000O4N9D402");

    // devInfo.setName("UnitreeDevice");
    devInfo.setName("Robot dog");
    devInfo.setManufacturer("Unitree");
    devInfo.setModel("Unitree GO2");
    devInfo.setSerialNumber("B42D4000O4N9D402");
    devInfo.setDeviceType(CreateType());

    return devInfo;
}

DeviceTypePtr UnitreeDevice::CreateType()
{
    return DeviceType("robotDog", "Robot dog", "", "daq.dog");
}

void UnitreeDevice::initChannels()
{
    auto microSecondsSinceDeviceStart = getMicroSecondsSinceDeviceStart();

    UnitreeChannelInit init{0, 1000, microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart};
    channel1 = createAndAddChannel<UnitreeChannel>(ioFolder, "ExCh1", init);
}

DeviceInfoPtr UnitreeDevice::onGetInfo()
{
    return CreateDeviceInfo();
}

std::chrono::microseconds UnitreeDevice::getMicroSecondsSinceDeviceStart() const
{
    auto currentTime = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime);
}

uint64_t UnitreeDevice::onGetTicksSinceOrigin()
{
    /*auto ticksSinceEpoch = microSecondsFromEpochToDeviceStart + getMicroSecondsSinceDeviceStart();
    return static_cast<SizeT>(ticksSinceEpoch.count());*/

    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

void UnitreeDevice::initDomain()
{
    startTime = std::chrono::steady_clock::now();
    auto startAbsTime = std::chrono::system_clock::now();

    microSecondsFromEpochToDeviceStart = std::chrono::duration_cast<std::chrono::microseconds>(startAbsTime.time_since_epoch());

    this->setDeviceDomain(DeviceDomain(UnitreeChannel::getResolution(), UnitreeChannel::getEpoch(), Unit("s", -1, "seconds", "time")));
}

void UnitreeDevice::acqLoop()
{
    daqNameThread("UnitreeDogThread");

    while (true)
    {
        std::unique_lock lock{mutex};
        cv.wait(lock, [this]() { return !queue.empty() || stopAcq; });

        if (stopAcq)
        {
            return;
        }

        auto data = std::move(queue.front());
        queue.pop();
        lock.unlock();

        forwardData(data);
    }
}

void UnitreeDevice::forwardDataCallback(std::vector<int16_t>&& data)
{
    if (CallbackQueueing)
    {
        std::unique_lock lock(mutex);
        stopAcq = false;
        queue.emplace(std::move(data));
        lock.unlock();
        cv.notify_all();
        return;
    }
    else
    {
        forwardData(data);
    }
}

void UnitreeDevice::forwardData(std::vector<int16_t>& data)
{
    // TODO: Maybe we need some multithreading locks here
    auto curTime = getMicroSecondsSinceDeviceStart();
    channel1.asPtr<IDogChannel>()->publishSamples(curTime, data);
}

// void UnitreeDevice::processData(int16_t data) const
// // void UnitreeDevice::processData(std::vector<int16_t> data) const
// {
//     using namespace std::chrono;
//     int64_t time = duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();

//     DataPacketPtr domainPacket = DataPacket(sigTime.getDescriptor(), 1);
//     int64_t* domainData = static_cast<int64_t*>(domainPacket.getRawData());
//     *domainData = time;

//     DataPacketPtr dp_F_fl = DataPacketWithDomain(domainPacket, sig_F_fl.getDescriptor(), 1);
//     DataPacketPtr dp_F_fr = DataPacketWithDomain(domainPacket, sig_F_fr.getDescriptor(), 1);
//     DataPacketPtr dp_F_rl = DataPacketWithDomain(domainPacket, sig_F_rl.getDescriptor(), 1);
//     DataPacketPtr dp_F_rr = DataPacketWithDomain(domainPacket, sig_F_rr.getDescriptor(), 1);

//     int16_t* data_F_fl = static_cast<int16_t*>(dp_F_fl.getRawData());
//     int16_t* data_F_fr = static_cast<int16_t*>(dp_F_fr.getRawData());
//     int16_t* data_F_rl = static_cast<int16_t*>(dp_F_rl.getRawData());
//     int16_t* data_F_rr = static_cast<int16_t*>(dp_F_rr.getRawData());

//     // std::cout << "data: " << data_F_fl << " " << data_F_fr << " " << data_F_rl << " " << data_F_rr << std::endl;

//     *data_F_fl = data;
//     *data_F_fr = data;
//     *data_F_rl = data;
//     *data_F_rr = data;

//     sigTime.sendPacket(domainPacket);
//     sig_F_fl.sendPacket(dp_F_fl);
//     sig_F_fr.sendPacket(dp_F_fr);
//     sig_F_rl.sendPacket(dp_F_rl);
//     sig_F_rr.sendPacket(dp_F_rr);
// }

END_NAMESPACE_UNITREE_MODULE
