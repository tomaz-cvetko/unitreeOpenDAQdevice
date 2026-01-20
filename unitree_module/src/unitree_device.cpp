#pragma once
#include <coreobjects/unit_factory.h>
#include <opendaq/data_packet_ptr.h>
#include <opendaq/device_domain_factory.h>
#include <opendaq/device_info_factory.h>
#include <opendaq/device_type_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/sync_component_private_ptr.h>
#include <opendaq/thread_name.h>

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
    constexpr size_t SampleRate = 1000;
}

UnitreeDevice::UnitreeDevice(const ContextPtr& ctx, const ComponentPtr& parent)
    : GenericDevice<>(ctx, parent, "DewesoftRobotics_B42D4000O4N9D402", nullptr, "RobotDog")
    , microSecondsFromEpochToDeviceStart(0)
    , stopAcq(false)
    , acqThread([this]() { this->acqLoop(); })
    , simThread([this]() { this->simLoop(); })
    , reader(std::nullopt)
{
    this->loggerComponent = ctx.getLogger().getOrAddComponent(UNITREE_MODULE_NAME);
    initDomain();
    initChannels();

    /****************************************/
    // Unitree DDS
    dds::domain::DomainParticipant participant(0);
    dds::topic::Topic<SensorDataListener::MsgType> topic(participant, SensorDataListener::GetTopicName());
    dds::sub::Subscriber subscriber(participant);

    reader.emplace(subscriber,
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
        std::unique_lock lock(mutex);
        stopAcq = true;
    }
    cv.notify_all();

    acqThread.join();
    simThread.join();
}

DeviceInfoPtr UnitreeDevice::CreateDeviceInfo()
{
    // auto devInfo = DeviceInfo("example://device");
    auto devInfo = DeviceInfo("daq.dog://DewesoftRobotics_B42D4000O4N9D402");

    // devInfo.setName("UnitreeDevice");
    devInfo.setName("Robot_dog");
    devInfo.setManufacturer("Unitree");
    devInfo.setModel("Unitree_GO2");
    devInfo.setSerialNumber("B42D4000O4N9D402");
    devInfo.setDeviceType(CreateType());

    return devInfo;
}

DeviceTypePtr UnitreeDevice::CreateType()
{
    return DeviceType("robotDog", "Robot_dog", "", "daq.dog");
}

void UnitreeDevice::initChannels()
{
    auto microSecondsSinceDeviceStart = getMicroSecondsSinceDeviceStart();

    UnitreeChannelInit init{0, static_cast<double>(SampleRate), microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart};
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

        std::vector<int16_t> data(std::move(queue.front()));
        queue.pop();
        lock.unlock();

        forwardData(data);
    }
}

void UnitreeDevice::simLoop()
{
    daqNameThread("UnitreeDogSimThread");

    auto generateData = [this](int num, int den, int16_t value)
    {
        size_t ssize = num * SampleRate / den;
        std::vector<int16_t> first(4 * ssize, value);
        forwardDataCallback(std::move(first));

        std::this_thread::sleep_for(std::chrono::milliseconds(1000 * num / den));
    };

    while (true)
    {
        {
            std::unique_lock lock(mutex);
            if (stopAcq)
            {
                return;
            }
        }

        generateData(1, 8, 2);
        generateData(1, 5, 3);
        generateData(1, 10, 4);
        generateData(1, 5, 3);
    }
}

void UnitreeDevice::forwardDataCallback(std::vector<int16_t>&& data)
{
    if (CallbackQueueing)
    {
        std::unique_lock lock(mutex);
        if (stopAcq)
        {
            return;
        }
        queue.emplace(std::move(data));
        lock.unlock();
        cv.notify_all();
        return;
    }
    else
    {
        if (stopAcq)
        {
            return;
        }
        forwardData(data);
    }
}

void UnitreeDevice::forwardData(std::vector<int16_t>& data)
{
    auto lock = this->getAcquisitionLock();
    // TODO: Maybe we need some multithreading locks here
    auto curTime = getMicroSecondsSinceDeviceStart();
    auto ch = channel1.asPtrOrNull<IDogChannel>();
    if (ch.assigned())
    {
        ch->publishSamples(curTime, data);
    }
}

END_NAMESPACE_UNITREE_MODULE
