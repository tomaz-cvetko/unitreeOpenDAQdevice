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
#include <iterator>
#include <vector>


BEGIN_NAMESPACE_UNITREE_MODULE

namespace
{
    // Samples waiting for the acquisition thread; 5000 = 10 s at 500 Hz. When full, the oldest are dropped
    // (they then show up as gaps in the time axis).
    constexpr size_t MaxQueuedSamples = 5000;
    // The acquisition thread wakes up at least this often, so partly filled packets are sent out on time.
    constexpr auto AcqWakeupPeriod = std::chrono::milliseconds(10);
}

UnitreeDevice::UnitreeDevice(const ContextPtr& ctx, const ComponentPtr& parent)
    : GenericDevice<>(ctx, parent, "DewesoftRobotics_B42D4000O4N9D402", nullptr, "RobotDog")
    , microSecondsFromEpochToDeviceStart(0)
    , stopAcq(false)
{
    // init() starts acqThread: if anything in it throws, the destructor is not called,
    // and destroying a joinable std::thread would terminate the process.
    try
    {
        init();
    }
    catch (...)
    {
        stopDds();
        stopAcqThread();
        throw;
    }
}

void UnitreeDevice::init()
{
    this->loggerComponent = this->context.getLogger().getOrAddComponent(UNITREE_MODULE_NAME);
    initDomain();
    initChannels();

    // Publishes the queued samples; started before DDS so nothing is waiting long when data arrives
    acqThread = std::thread([this]() { this->acqLoop(); });
    std::cout << "creating device\n";

    // Unitree DDS
    dds::domain::DomainParticipant participant(0);
    dds::topic::Topic<SensorDataListenerLowState::MsgType> topic(participant, SensorDataListenerLowState::GetTopicName());
    dds::sub::Subscriber subscriber(participant);

    low_state_listener = std::make_unique<SensorDataListenerLowState>(
        [this](SensorDataListenerLowState::BufferS&& buffer) {
            this->forwardDataCallback(std::move(buffer));
        }
    );

    // The listener only queues samples, but leave DDS room for short scheduling hiccups
    // (the default reader history keeps just the newest sample and silently overwrites the rest).
    auto readerQos = subscriber.default_datareader_qos();
    readerQos << dds::core::policy::History::KeepLast(64);

    reader.emplace(
        subscriber,
        topic,
        readerQos,
        low_state_listener.get(),
        dds::core::status::StatusMask::data_available()
    );

    std::cout << "SensorDataListenerLowState::GetTopicName(): " << SensorDataListenerLowState::GetTopicName() << std::endl;
    std::cout << "Listening for Unitree Go2 sensor data on topic '" << SensorDataListenerLowState::GetTopicName() << "'..." << std::endl;
}

UnitreeDevice::~UnitreeDevice()
{
    // Stop the data source first, then the thread that consumes it.
    stopDds();
    stopAcqThread();
}

void UnitreeDevice::stopDds()
{
    if (!reader)
        return;

    // Closing the reader waits for a listener callback that is already running to finish,
    // and no new callbacks into this (half-destroyed) device can start afterwards.
    try
    {
        reader->close();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Closing the DDS reader failed: " << e.what() << std::endl;
    }
    reader.reset();
}

void UnitreeDevice::stopAcqThread()
{
    {
        // Must be the same mutex acqLoop waits on, otherwise the wake-up can be missed.
        std::lock_guard lock(mutex);
        stopAcq = true;
    }
    cv.notify_all();

    if (acqThread.joinable())
        acqThread.join();
}

DeviceInfoPtr UnitreeDevice::CreateDeviceInfo()
{
    auto devInfo = DeviceInfo("daq.dog://DewesoftRobotics_B42D4000O4N9D402");

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

    std::vector<SensorDataListenerLowState::BufferS> samples;
    uint64_t reportedDrops = 0;

    while (true)
    {
        uint64_t drops;
        {
            std::unique_lock lock{mutex};
            cv.wait_for(lock, AcqWakeupPeriod, [this]() { return !queue.empty() || stopAcq; });

            if (stopAcq)
                return;

            samples.assign(std::make_move_iterator(queue.begin()), std::make_move_iterator(queue.end()));
            queue.clear();
            drops = droppedQueueFull;
        }

        if (drops != reportedDrops)
        {
            std::cout << "Acquisition fell behind: " << (drops - reportedDrops) << " samples dropped (queue full)" << std::endl;
            reportedDrops = drops;
        }

        // Also called without samples, so the channel can send a partly filled packet that has waited long enough
        channel1.asPtr<IDogChannel>()->publishSamples(getMicroSecondsSinceDeviceStart(), samples);
    }
}

void UnitreeDevice::forwardDataCallback(SensorDataListenerLowState::BufferS&& data)
{
    // Taken here, not when the sample is processed, so time spent in the queue does not count as clock offset
    data.arrivalTime = getMicroSecondsSinceDeviceStart();
    {
        std::lock_guard lock(mutex);
        if (queue.size() >= MaxQueuedSamples)
        {
            queue.pop_front();
            ++droppedQueueFull;
        }
        queue.push_back(std::move(data));
    }
    cv.notify_one();
}

END_NAMESPACE_UNITREE_MODULE
