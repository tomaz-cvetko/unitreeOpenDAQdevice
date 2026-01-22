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
    // initSignals();
    std::cout << "creating device\n";

    /****************************************/
    // Unitree DDS
    dds::domain::DomainParticipant participant(0);
    dds::topic::Topic<SensorDataListener::MsgType> topic(participant, SensorDataListener::GetTopicName());
    // dds::topic::Topic<SensorDataListener::MsgType> topic(participant, "rt/lowstate");
    dds::sub::Subscriber subscriber(participant);

    reader.emplace(
        subscriber,
        topic,
        subscriber.default_datareader_qos(),
        new SensorDataListener([this](std::vector<int16_t>&& data) {
            this->forwardDataCallback(std::move(data)); }),
        // new SensorDataListener(),
        dds::core::status::StatusMask::data_available()
    );
    std::cout << "SensorDataListener::GetTopicName(): " << SensorDataListener::GetTopicName() << std::endl;
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

    // UnitreeChannelInit init{0, 1000, microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart};
    UnitreeChannelInit init{0, 500, microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart};
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

END_NAMESPACE_UNITREE_MODULE
