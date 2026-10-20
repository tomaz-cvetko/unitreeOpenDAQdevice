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
    // std::cout << "Hello from UnitreeDevice::UnitreeDevice\n";

    this->loggerComponent = ctx.getLogger().getOrAddComponent(UNITREE_MODULE_NAME);
    initDomain();
    initChannels();
    // initSignals();
    std::cout << "creating device\n";

    /****************************************/
    // Unitree DDS
    dds::domain::DomainParticipant participant(0);
    dds::topic::Topic<SensorDataListenerLowState::MsgType> topic(participant, SensorDataListenerLowState::GetTopicName());
    // dds::topic::Topic<SensorDataListenerLowState::MsgType> topic(participant, "rt/lowstate");
    dds::sub::Subscriber subscriber(participant);

    low_state_listener = std::make_unique<SensorDataListenerLowState>(
        [this](SensorDataListenerLowState::BufferS&& buffer) {
            this->forwardDataCallback(std::move(buffer));
        }
    );

    reader.emplace(
        subscriber,
        topic,
        subscriber.default_datareader_qos(),
        low_state_listener.get(),
        dds::core::status::StatusMask::data_available()
    );

    /*reader.emplace(
        subscriber,
        topic,
        subscriber.default_datareader_qos(),
        // new SensorDataListenerLowState([this](std::vector<int16_t>&& data) {
        //     this->forwardDataCallback(std::move(data)); }),
        [this](SensorDataListenerLowState::BufferS&& buffer) {
            this->forwardDataCallback(std::move(buffer)); },
        // new SensorDataListenerLowState(),
        dds::core::status::StatusMask::data_available()
    );*/
    std::cout << "SensorDataListenerLowState::GetTopicName(): " << SensorDataListenerLowState::GetTopicName() << std::endl;
    std::cout << "Listening for Unitree Go2 sensor data on topic '" << SensorDataListenerLowState::GetTopicName() << "'..." << std::endl;
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
    // std::cout << "Hello from UnitreeDevice::CreateDeviceInfo\n";
    
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
    // std::cout << "Hello from UnitreeDevice::CreateType\n";

    return DeviceType("robotDog", "Robot dog", "", "daq.dog");
}

void UnitreeDevice::initChannels()
{
    // std::cout << "Hello from UnitreeDevice::initChannels\n";

    auto microSecondsSinceDeviceStart = getMicroSecondsSinceDeviceStart();

    // UnitreeChannelInit init{0, 1000, microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart};
    UnitreeChannelInit init{0, 500, microSecondsSinceDeviceStart, microSecondsFromEpochToDeviceStart};
    channel1 = createAndAddChannel<UnitreeChannel>(ioFolder, "ExCh1", init);
}

DeviceInfoPtr UnitreeDevice::onGetInfo()
{
    // std::cout << "Hello from UnitreeDevice::onGetInfo\n";

    return CreateDeviceInfo();
}

std::chrono::microseconds UnitreeDevice::getMicroSecondsSinceDeviceStart() const
{
    // std::cout << "Hello from UnitreeDevice::getMicroSecondsSinceDeviceStart\n";

    auto currentTime = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime);
}

uint64_t UnitreeDevice::onGetTicksSinceOrigin()
{
    // std::cout << "Hello from UnitreeDevice::onGetTicksSinceOrigin\n";

    /*auto ticksSinceEpoch = microSecondsFromEpochToDeviceStart + getMicroSecondsSinceDeviceStart();
    return static_cast<SizeT>(ticksSinceEpoch.count());*/

    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

void UnitreeDevice::initDomain()
{
    // std::cout << "Hello from UnitreeDevice::initDomain\n";

    startTime = std::chrono::steady_clock::now();
    auto startAbsTime = std::chrono::system_clock::now();

    microSecondsFromEpochToDeviceStart = std::chrono::duration_cast<std::chrono::microseconds>(startAbsTime.time_since_epoch());

    this->setDeviceDomain(DeviceDomain(UnitreeChannel::getResolution(), UnitreeChannel::getEpoch(), Unit("s", -1, "seconds", "time")));
}

void UnitreeDevice::acqLoop()
{
    // std::cout << "Hello from UnitreeDevice::acqLoop\n";

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

// void UnitreeDevice::forwardDataCallback(std::vector<int16_t>&& data)
void UnitreeDevice::forwardDataCallback(SensorDataListenerLowState::BufferS&& data)
{
    // std::cout << "Hello from UnitreeDevice::forwardDataCallback\n";

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

// void UnitreeDevice::forwardData(std::vector<int16_t>& data)
void UnitreeDevice::forwardData(SensorDataListenerLowState::BufferS& data)
{
    // std::cout << "Hello from UnitreeDevice::forwardData\n";

    // TODO: Maybe we need some multithreading locks here
    auto curTime = getMicroSecondsSinceDeviceStart();
    channel1.asPtr<IDogChannel>()->publishSamples(curTime, data);
}

END_NAMESPACE_UNITREE_MODULE
