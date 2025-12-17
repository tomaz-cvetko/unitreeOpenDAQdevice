#include <coreobjects/callable_info_factory.h>
#include <coreobjects/unit_factory.h>
#include <fmt/format.h>
#include <opendaq/data_rule_factory.h>
#include <opendaq/packet_factory.h>
#include <opendaq/signal_factory.h>
#include <unitree_module/unitree_channel.h>
#include <chrono>

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
    initProperties();
    createSignals();
    buildSignalDescriptors();
}

void UnitreeChannel::initProperties()
{
}

void UnitreeChannel::publishSamples(std::chrono::microseconds curTime, std::vector<int16_t>& data)
{
    auto lock = this->getAcquisitionLock();

    const size_t bufferSize = data.size();
    assert(bufferSize % 4 == 0);

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
    forceFLsignal.sendPacket(packets[0]);
    forceFRsignal.sendPacket(packets[1]);
    forceRLsignal.sendPacket(packets[2]);
    forceRRsignal.sendPacket(packets[3]);

    counter += sampleCount;
}

std::string UnitreeChannel::getEpoch()
{
    const std::time_t epochTime = std::chrono::system_clock::to_time_t(std::chrono::time_point<std::chrono::system_clock>{});

    char buf[48];
    strftime(buf, sizeof buf, "%Y-%m-%dT%H:%M:%SZ", gmtime(&epochTime));

    return { buf };
}

RatioPtr UnitreeChannel::getResolution()
{
    return Ratio(1, 1000000);
}

uint64_t UnitreeChannel::getSamplesSinceStart(std::chrono::microseconds time) const
{
    return counter;
}

Int UnitreeChannel::getDeltaT(const double sr) const
{
    const double tickPeriod = getResolution();
    const double samplePeriod = 1.0 / sr;
    return static_cast<Int>(std::round(samplePeriod / tickPeriod));
}

void UnitreeChannel::buildSignalDescriptors()
{
    const auto valueDescriptorForce = DataDescriptorBuilder().setSampleType(SampleType::Int16).setUnit(Unit("N", -1));

    forceFLsignal.setDescriptor(valueDescriptorForce.build());
    forceFRsignal.setDescriptor(valueDescriptorForce.build());
    forceRLsignal.setDescriptor(valueDescriptorForce.build());
    forceRRsignal.setDescriptor(valueDescriptorForce.build());

    // delta, start, tickResolution, unit, origin
    deltaT = getDeltaT(sampleRate);

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
    timeSignal = createAndAddSignal(fmt::format("Dog Time"), nullptr, false);

    forceFLsignal = createAndAddSignal(fmt::format("FL force"));
    forceFLsignal.setDomainSignal(timeSignal);

    forceFRsignal = createAndAddSignal(fmt::format("FR force"));
    forceFRsignal.setDomainSignal(timeSignal);

    forceRLsignal = createAndAddSignal(fmt::format("RL force"));
    forceRLsignal.setDomainSignal(timeSignal);

    forceRRsignal = createAndAddSignal(fmt::format("RR force"));
    forceRRsignal.setDomainSignal(timeSignal);
}

END_NAMESPACE_UNITREE_MODULE
