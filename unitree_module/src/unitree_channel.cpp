#include <coreobjects/callable_info_factory.h>
#include <coreobjects/unit_factory.h>
#include <fmt/format.h>
#include <opendaq/data_rule_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/signal_factory.h>
#include <unitree_module/unitree_channel.h>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <chrono>
#include <iostream>
#include <type_traits>

BEGIN_NAMESPACE_UNITREE_MODULE

namespace
{
    // LowState::tick counts milliseconds ("Used for 1 ms timing, incrementing in steps of 1 ms").
    constexpr uint64_t DogTickUs = 1000;
    // A larger forward jump (or any backward jump, e.g. the dog rebooted) re-anchors the time axis to the PC clock.
    constexpr int64_t MaxDogTickJumpMs = 5000;

    // Samples per packet: 25 samples = 50 ms at 500 Hz. Sending one packet per signal per sample (74 packets
    // every 2 ms) costs too much CPU; with blocks it is 74 packets every 50 ms.
    constexpr size_t BlockSize = 25;
    // A block that is not full yet is sent anyway after this long, so the batching adds at most ~50 ms latency.
    constexpr uint64_t MaxBlockAgeUs = 50'000;

    // Clock drift correction: evaluated every 10 s; at most 25 slips (= 50 ms) per window, i.e. up to 5000 ppm,
    // far more than a crystal drifts (typically < 100 ppm), and spread at least 100 samples (0.2 s) apart.
    constexpr uint64_t DriftWindowUs = 10'000'000;
    constexpr int64_t MaxSlipsPerWindow = 25;
    constexpr uint64_t MinSamplesBetweenSlips = 100;

    template <typename T>
    constexpr SampleType sampleTypeOf()
    {
        if constexpr (std::is_same_v<T, float>)
            return SampleType::Float32;
        else if constexpr (std::is_same_v<T, int16_t>)
            return SampleType::Int16;
        else if constexpr (std::is_same_v<T, uint8_t>)
            return SampleType::UInt8;
        else if constexpr (std::is_same_v<T, uint16_t>)
            return SampleType::UInt16;
        else
            static_assert(!sizeof(T), "add the openDAQ sample type for this C++ type");
    }

    // Unitree orders the legs FR, FL, RR, RL (foot_force[], motor_state[]); 3 motors per leg
    const std::array<const char*, 12> MotorNames = {"FR1", "FR2", "FR3", "FL1", "FL2", "FL3", "RR1", "RR2", "RR3", "RL1", "RL2", "RL3"};
}

UnitreeChannel::UnitreeChannel(const ContextPtr& context,
                               const ComponentPtr& parent,
                               const StringPtr& localId,
                               const UnitreeChannelInit& init)
    : ChannelImpl(FunctionBlockType("UnitreeChannel", fmt::format("DogCh{}", init.index + 1), ""), context, parent, localId)
    , index(init.index)
    , startTime(init.startTime)
    , microSecondsFromEpochToStartTime(init.microSecondsFromEpochToStartTime)
    , sampleRate(init.sampleRate)
{
    initProperties();
    createSignals();
    block.reserve(BlockSize);
}

void UnitreeChannel::initProperties()
{
}

template <typename T, typename Getter>
void UnitreeChannel::addValueSignal(const std::string& name, const std::string& unit, Getter getter)
{
    ValueSignal valueSignal;
    valueSignal.signal = createAndAddSignal(name);
    valueSignal.signal.setDomainSignal(timeSignal);
    valueSignal.descriptor = DataDescriptorBuilder().setSampleType(sampleTypeOf<T>()).setUnit(Unit(unit, -1)).build();
    valueSignal.signal.setDescriptor(valueSignal.descriptor);
    valueSignal.write = [getter](void* packetData, size_t sampleIndex, const DogSample& sample)
    {
        static_cast<T*>(packetData)[sampleIndex] = static_cast<T>(getter(sample));
    };
    valueSignals.push_back(std::move(valueSignal));
}

void UnitreeChannel::createSignals()
{
    deltaT = getDeltaT(sampleRate);  // In number of ticks

    // PacketOffset + rule.delta * sampleIndex + rule.start -> ticks since origin
    // absolute time = ticksSinceOrigin * tickResolution + origin
    timeSignal = createAndAddSignal(fmt::format("Dog_Time"), nullptr, true);
    timeSignal.setDescriptor(DataDescriptorBuilder()
                                 .setSampleType(SampleType::Int64)
                                 .setUnit(Unit("s", -1, "seconds", "time"))
                                 .setTickResolution(getResolution())
                                 .setRule(LinearDataRule(deltaT, 0))
                                 .setOrigin(getEpoch())
                                 .build());

    // Names, units, sample types and order are what clients (e.g. DewesoftX setups) see: keep them stable.
    using S = const DogSample&;

    addValueSignal<int16_t>("FL_force", "N", [](S s) { return s.forces[1]; });
    addValueSignal<int16_t>("FR_force", "N", [](S s) { return s.forces[0]; });
    addValueSignal<int16_t>("RL_force", "N", [](S s) { return s.forces[3]; });
    addValueSignal<int16_t>("RR_force", "N", [](S s) { return s.forces[2]; });

    addValueSignal<uint8_t>("battery", "%", [](S s) { return s.battery; });

    addValueSignal<float>("roll", "rad", [](S s) { return s.IMU.rpy()[0]; });
    addValueSignal<float>("pitch", "rad", [](S s) { return s.IMU.rpy()[1]; });
    addValueSignal<float>("yaw", "rad", [](S s) { return s.IMU.rpy()[2]; });
    addValueSignal<float>("accelerometer_X", "m/s^2", [](S s) { return s.IMU.accelerometer()[0]; });
    addValueSignal<float>("accelerometer_Y", "m/s^2", [](S s) { return s.IMU.accelerometer()[1]; });
    addValueSignal<float>("accelerometer_Z", "m/s^2", [](S s) { return s.IMU.accelerometer()[2]; });
    addValueSignal<float>("gyroscope_X", "rad/s", [](S s) { return s.IMU.gyroscope()[0]; });
    addValueSignal<float>("gyroscope_Y", "rad/s", [](S s) { return s.IMU.gyroscope()[1]; });
    addValueSignal<float>("gyroscope_Z", "rad/s", [](S s) { return s.IMU.gyroscope()[2]; });
    addValueSignal<float>("quaternion_W", "", [](S s) { return s.IMU.quaternion()[0]; });
    addValueSignal<float>("quaternion_X", "", [](S s) { return s.IMU.quaternion()[1]; });
    addValueSignal<float>("quaternion_Y", "", [](S s) { return s.IMU.quaternion()[2]; });
    addValueSignal<float>("quaternion_Z", "", [](S s) { return s.IMU.quaternion()[3]; });

    addValueSignal<float>("current", "A", [](S s) { return s.current; });
    addValueSignal<float>("voltage", "V", [](S s) { return s.voltage; });
    addValueSignal<uint8_t>("CPU_temperature", "°C", [](S s) { return s.CPUtemp; });
    addValueSignal<uint8_t>("IMU_temperature", "°C", [](S s) { return s.IMU.temperature(); });
    addValueSignal<uint16_t>("LR_fan_frequency", "Hz", [](S s) { return s.fanFrequency[0]; });
    addValueSignal<uint16_t>("RR_fan_frequency", "Hz", [](S s) { return s.fanFrequency[1]; });
    addValueSignal<uint16_t>("front_fan_frequency", "Hz", [](S s) { return s.fanFrequency[2]; });

    for (size_t i = 0; i < MotorNames.size(); ++i)
        addValueSignal<float>(fmt::format("motor{}_q", MotorNames[i]), "rad", [i](S s) { return s.motors[i].q(); });
    for (size_t i = 0; i < MotorNames.size(); ++i)
        addValueSignal<float>(fmt::format("motor{}_dq", MotorNames[i]), "", [i](S s) { return s.motors[i].dq(); });
    for (size_t i = 0; i < MotorNames.size(); ++i)
        addValueSignal<float>(fmt::format("motor{}_tau", MotorNames[i]), "", [i](S s) { return s.motors[i].tau_est(); });
    for (size_t i = 0; i < MotorNames.size(); ++i)
        addValueSignal<uint8_t>(fmt::format("motor{}_temperature", MotorNames[i]), "°C", [i](S s) { return s.motors[i].temperature(); });
}

void UnitreeChannel::anchorTime(uint32_t dogTick, uint64_t pcTimeUs)
{
    timeAnchored = true;
    anchorTimeUs = pcTimeUs;
    lastDogTick = dogTick;
    dogTicksSinceAnchor = 0;

    windowMinOffsetUs = INT64_MAX;
    windowStartUs = pcTimeUs;
    pendingSlips = 0;
}

std::optional<uint64_t> UnitreeChannel::getSampleTime(uint32_t dogTick, uint64_t pcTimeUs)
{
    if (!timeAnchored)
    {
        anchorTime(dogTick, pcTimeUs);
        std::cout << "Time axis anchored: dog tick " << dogTick << " = PC time " << pcTimeUs << " us" << std::endl;
        return anchorTimeUs;
    }

    // Unsigned subtraction is wrap-safe; reading it as signed tells forward from backward jumps.
    const int64_t step = static_cast<int32_t>(dogTick - lastDogTick);
    if (step == 0)
    {
        // Same tick twice: a duplicate message, not a new measurement.
        ++droppedDuplicates;
        return std::nullopt;
    }
    if (step < 0 || step > MaxDogTickJumpMs)
    {
        std::cout << "Dog tick jumped from " << lastDogTick << " to " << dogTick << " (" << step
                  << " ms); re-anchoring the time axis to the PC clock" << std::endl;
        anchorTime(dogTick, pcTimeUs);
        return anchorTimeUs;
    }

    lastDogTick = dogTick;
    dogTicksSinceAnchor += static_cast<uint64_t>(step);
    return anchorTimeUs + dogTicksSinceAnchor * DogTickUs;
}

void UnitreeChannel::publishSamples(std::chrono::microseconds curTime, const std::vector<DogSample>& samples)
{
    auto lock = this->getAcquisitionLock();

    const uint64_t pcTimeUs = static_cast<uint64_t>((microSecondsFromEpochToStartTime + curTime).count());
    for (const auto& sample : samples)
    {
        const uint64_t arrivalUs = static_cast<uint64_t>((microSecondsFromEpochToStartTime + sample.arrivalTime).count());
        const auto sampleTime = getSampleTime(sample.tick, arrivalUs);
        if (!sampleTime)
            continue;

        ++samplesSinceSlip;
        const bool slipNow = pendingSlips != 0 && samplesSinceSlip >= MinSamplesBetweenSlips;
        if (slipNow && pendingSlips < 0)
        {
            // Timestamps run ahead of the PC clock: drop this sample; the next one takes its time slot.
            anchorTimeUs -= deltaT;
            ++pendingSlips;
            samplesSinceSlip = 0;
            continue;
        }

        appendSample(sample, *sampleTime, arrivalUs);
        trackClockOffset(static_cast<int64_t>(arrivalUs) - static_cast<int64_t>(*sampleTime), arrivalUs);

        if (slipNow && pendingSlips > 0)
        {
            // Timestamps lag behind the PC clock: repeat this sample, which moves all later samples one period later.
            anchorTimeUs += deltaT;
            appendSample(sample, *sampleTime + deltaT, arrivalUs);
            --pendingSlips;
            samplesSinceSlip = 0;
        }
    }

    if (!block.empty() && pcTimeUs - blockFirstArrivalUs >= MaxBlockAgeUs)
        sendBlock();
}

void UnitreeChannel::trackClockOffset(int64_t offsetUs, uint64_t arrivalUs)
{
    windowMinOffsetUs = std::min(windowMinOffsetUs, offsetUs);
    if (arrivalUs - windowStartUs < DriftWindowUs)
        return;

    // Start a new correction only when the previous one is done; otherwise this window's minimum
    // was measured while it was still being applied.
    const auto period = static_cast<int64_t>(deltaT);
    if (pendingSlips == 0 && std::llabs(windowMinOffsetUs) >= period)
    {
        pendingSlips = std::clamp(windowMinOffsetUs / period, -MaxSlipsPerWindow, MaxSlipsPerWindow);
        std::cout << "Clock drift correction: timestamps are " << std::llabs(windowMinOffsetUs) / 1000.0 << " ms "
                  << (windowMinOffsetUs > 0 ? "behind" : "ahead of") << " the PC clock; "
                  << (pendingSlips > 0 ? "repeating " : "dropping ") << std::llabs(pendingSlips) << " sample(s)" << std::endl;
    }

    windowMinOffsetUs = INT64_MAX;
    windowStartUs = arrivalUs;
}

void UnitreeChannel::appendSample(const DogSample& sample, uint64_t sampleTime, uint64_t pcTimeUs)
{
    // The linear time rule needs equally spaced samples within a packet:
    // a gap (or a re-anchored time axis) ends the current block and starts a new one.
    if (!block.empty() && sampleTime != blockStartTime + block.size() * deltaT)
        sendBlock();

    if (block.empty())
    {
        blockStartTime = sampleTime;
        blockFirstArrivalUs = pcTimeUs;
    }
    block.push_back(sample);

    if (block.size() >= BlockSize)
        sendBlock();
}

void UnitreeChannel::sendBlock()
{
    const size_t sampleCount = block.size();

    auto domainPacket = DataPacket(timeSignal.getDescriptor(), sampleCount, blockStartTime);
    timeSignal.sendPacket(domainPacket);

    for (const auto& valueSignal : valueSignals)
    {
        auto packet = DataPacketWithDomain(domainPacket, valueSignal.descriptor, sampleCount);
        void* packetData = packet.getRawData();
        for (size_t i = 0; i < sampleCount; ++i)
            valueSignal.write(packetData, i, block[i]);
        valueSignal.signal.sendPacket(packet);
    }

    block.clear();
}

std::string UnitreeChannel::getEpoch()
{
    const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});

    char buf[48];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));

    return {buf};
}

RatioPtr UnitreeChannel::getResolution()
{
    return Ratio(1, 1000000);
}

Int UnitreeChannel::getDeltaT(const double sr) const
{
    const double tickPeriod = getResolution();
    const double samplePeriod = 1.0 / sr;
    return static_cast<Int>(std::round(samplePeriod / tickPeriod));
}

END_NAMESPACE_UNITREE_MODULE
