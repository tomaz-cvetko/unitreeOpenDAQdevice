#include <coreobjects/callable_info_factory.h>
#include <coreobjects/unit_factory.h>
#include <fmt/format.h>
#include <opendaq/data_rule_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/signal_factory.h>
#include <unitree_module/unitree_channel.h>
#include <chrono>
#include <iostream>

BEGIN_NAMESPACE_UNITREE_MODULE

UnitreeChannel::UnitreeChannel(const ContextPtr& context,
                               const ComponentPtr& parent,
                               const StringPtr& localId,
                               const UnitreeChannelInit& init)
    : ChannelImpl(FunctionBlockType("UnitreeChannel", fmt::format("DogCh{}", init.index + 1), ""), context, parent, localId)
    , index(init.index)
    , startTime(init.startTime)
    , microSecondsFromEpochToStartTime(init.microSecondsFromEpochToStartTime)
    , samplesGenerated(0)
    , counter(0)
    , sampleRate(init.sampleRate)
{
    // std::cout << "Hello from UnitreeChannel::UnitreeChannel\n";

    initProperties();
    createSignals();
    buildSignalDescriptors();
}

void UnitreeChannel::initProperties()
{
    // std::cout << "Hello from UnitreeChannel::initProperties\n";
}

uint64_t UnitreeChannel::getSamplesSinceStart(std::chrono::microseconds time) const
{
    // std::cout << "Hello from UnitreeChannel::getSamplesSinceStart\n";

    const uint64_t samplesSinceStart =
        static_cast<uint64_t>(std::trunc(static_cast<double>((time - startTime).count()) / 1'000'000.0 * sampleRate));
    //     static_cast<uint64_t>(std::trunc(static_cast<double>((time - startTime).count()) / 500.0 * sampleRate));
    return samplesSinceStart;
}

uint64_t UnitreeChannel::getSamplesSinceStart(std::chrono::microseconds time) const
{
    const uint64_t samplesSinceStart =
        static_cast<uint64_t>(std::trunc(static_cast<double>((time - startTime).count()) / 1'000'000.0 * sampleRate));
    return samplesSinceStart;
}

void UnitreeChannel::publishSamples(std::chrono::microseconds curTime, std::vector<int16_t>& data)
{
    // std::cout << "Hello from UnitreeChannel::publishSamples\n";

    if (startTick == 0)
    {
        startTick = getSamplesSinceStart(curTime);
        std::cout << "startTick: " << startTick << std::endl;
    }

    auto lock = this->getAcquisitionLock();

    const size_t bufferSize = data.size();
    assert(bufferSize % 4 == 0);

    // Number of samples in a synchronous & gapless scenario
    const uint64_t samplesSinceStart = getSamplesSinceStart(curTime);
    // const uint64_t neededSamples = samplesSinceStart - samplesGenerated;

    const size_t sampleCount = bufferSize / 4;

    // TODO: I have no idea what timestamps are appropriate here
    auto domainPacket = DataPacket(timeSignal.getDescriptor(), sampleCount, curTime.count());
    std::vector<DataPacketPtr> packets;
    packets.reserve(4);
    packets.push_back(DataPacketWithDomain(domainPacket, forceFLsignal.getDescriptor(), sampleCount));  // flPacket
    packets.push_back(DataPacketWithDomain(domainPacket, forceFRsignal.getDescriptor(), sampleCount));  // frPacket
    packets.push_back(DataPacketWithDomain(domainPacket, forceRLsignal.getDescriptor(), sampleCount));  // rlPacket
    packets.push_back(DataPacketWithDomain(domainPacket, forceRRsignal.getDescriptor(), sampleCount));  // rrPacket

    // NOTE: If this is too slow, we can optimize the data structure
    for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        for (size_t i = 0; i < 4; ++i)
        {
            int16_t* packetData = static_cast<int16_t*>(packets[i].getRawData());
            packetData[sampleIndex] = data[4 * sampleIndex + i];
        }
    }

    timeSignal.sendPacket(domainPacket);
    forceFRsignal.sendPacket(packets[0]);
    forceFLsignal.sendPacket(packets[1]);
    forceRRsignal.sendPacket(packets[2]);
    forceRLsignal.sendPacket(packets[3]);

    counter += sampleCount;
    samplesGenerated = samplesSinceStart;
}

std::string UnitreeChannel::getEpoch()
{
    // std::cout << "Hello from UnitreeChannel::getEpoch\n";
    
    const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});

    char buf[48];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));

    return {buf};
}

RatioPtr UnitreeChannel::getResolution()
{
    // std::cout << "Hello from UnitreeChannel::getResolution\n";

    return Ratio(1, 1000000);
}

Int UnitreeChannel::getDeltaT(const double sr) const
{
    // std::cout << "Hello from UnitreeChannel::getDeltaT\n";

    const double tickPeriod = getResolution();
    const double samplePeriod = 1.0 / sr;
    return static_cast<Int>(std::round(samplePeriod / tickPeriod));
}

void UnitreeChannel::buildSignalDescriptors()
{
    // std::cout << "Hello from UnitreeChannel::buildSignalDescriptors\n";

    const auto valueDescriptorForce = DataDescriptorBuilder().setSampleType(SampleType::Int16).setUnit(Unit("N", -1));
    const auto valueDescriptorMotorPosition = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("°", -1));

    forceFLsignal.setDescriptor(valueDescriptorForce.build());
    forceFRsignal.setDescriptor(valueDescriptorForce.build());
    forceRLsignal.setDescriptor(valueDescriptorForce.build());
    forceRRsignal.setDescriptor(valueDescriptorForce.build());

    // delta, start, tickResolution, unit, origin
    deltaT = getDeltaT(sampleRate);  // In number of ticks

    // PacketOffset
    // PacketOffset + rule.delta * sampleIndex + rule.start -> ticks since origin
    // absolute time = ticksSinceOrigin * tickResolution + origin

    const auto timeDescriptor = DataDescriptorBuilder()
                                    .setSampleType(SampleType::Int64)
                                    .setUnit(Unit("s", -1, "seconds", "time"))
                                    .setTickResolution(getResolution())
                                    .setRule(LinearDataRule(deltaT, 0))
                                    .setOrigin(getEpoch());

    timeSignal.setDescriptor(timeDescriptor.build());
}

void UnitreeChannel::createSignals()
{
    timeSignal = createAndAddSignal(fmt::format("Dog_Time"), nullptr, false);

    forceFLsignal = createAndAddSignal(fmt::format("FL_force"));
    forceFLsignal.setDomainSignal(timeSignal);

    forceFRsignal = createAndAddSignal(fmt::format("FR_force"));
    forceFRsignal.setDomainSignal(timeSignal);

    forceRLsignal = createAndAddSignal(fmt::format("RL_force"));
    forceRLsignal.setDomainSignal(timeSignal);

    forceRRsignal = createAndAddSignal(fmt::format("RR_force"));
    forceRRsignal.setDomainSignal(timeSignal);
}

END_NAMESPACE_UNITREE_MODULE



/*
Hello from UnitreeChannel::getEpoch
Hello from UnitreeChannel::getResolution
Hello from UnitreeChannel::UnitreeChannel
Hello from UnitreeChannel::initProperties
Hello from UnitreeChannel::createSignals
Hello from UnitreeChannel::buildSignalDescriptors
Hello from UnitreeChannel::getDeltaT
Hello from UnitreeChannel::getResolution
Hello from UnitreeChannel::getResolution
Hello from UnitreeChannel::getEpoch
creating device
Hello from SensorDataListener!
SensorDataListener::GetTopicName(): rt/lowstate
Listening for Unitree Go2 sensor data on topic 'rt/lowstate'...
Press "enter" to exit the application...
Hello from UnitreeChannel::publishSamples
Hello from UnitreeChannel::getSamplesSinceStart
Hello from UnitreeChannel::publishSamples
Hello from UnitreeChannel::getSamplesSinceStart
Hello from UnitreeChannel::publishSamples
Hello from UnitreeChannel::getSamplesSinceStart
Hello from UnitreeChannel::publishSamples
Hello from UnitreeChannel::getSamplesSinceStart
*/