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

namespace
{
    // LowState::tick counts milliseconds ("Used for 1 ms timing, incrementing in steps of 1 ms").
    constexpr uint64_t DogTickUs = 1000;
    // A larger forward jump (or any backward jump, e.g. the dog rebooted) re-anchors the time axis to the PC clock.
    constexpr int64_t MaxDogTickJumpMs = 5000;
}

void UnitreeChannel::anchorTime(uint32_t dogTick, uint64_t pcTimeUs)
{
    timeAnchored = true;
    anchorTimeUs = pcTimeUs;
    lastDogTick = dogTick;
    dogTicksSinceAnchor = 0;
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

// void UnitreeChannel::publishSamples(std::chrono::microseconds curTime, std::vector<int16_t>& data)
void UnitreeChannel::publishSamples(std::chrono::microseconds curTime, SensorDataListenerLowState::BufferS& data)
{
    // std::cout << "Hello from UnitreeChannel::publishSamples\n";

    auto lock = this->getAcquisitionLock();

    const uint64_t pcTimeUs = static_cast<uint64_t>((microSecondsFromEpochToStartTime + curTime).count());
    const auto sampleTime = getSampleTime(data.tick, pcTimeUs);
    if (!sampleTime)
        return;

    // const size_t bufferSize = data.size();
    // assert(bufferSize % 4 == 0);

    constexpr size_t channelCount = 4;
    constexpr size_t sampleCount = 1; // ONE sample per BufferS

    // const size_t sampleCount = bufferSize / 4;

    // One sample per packet, so each packet's offset is that sample's time (see getSampleTime)
    const auto packetTime = *sampleTime;
    auto domainPacket = DataPacket(timeSignal.getDescriptor(), sampleCount, packetTime);
    
    std::vector<DataPacketPtr> packets;
    packets.reserve(73);

    packets.push_back(DataPacketWithDomain(domainPacket, forceFLsignal.getDescriptor(), sampleCount));  // flPacket
    packets.push_back(DataPacketWithDomain(domainPacket, forceFRsignal.getDescriptor(), sampleCount));  // frPacket
    packets.push_back(DataPacketWithDomain(domainPacket, forceRLsignal.getDescriptor(), sampleCount));  // rlPacket
    packets.push_back(DataPacketWithDomain(domainPacket, forceRRsignal.getDescriptor(), sampleCount));  // rrPacket
    
    packets.push_back(DataPacketWithDomain(domainPacket, batterySignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, motor01qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor02qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor03qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor04qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor05qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor06qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor07qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor08qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor09qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor10qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor11qSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor12qSignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, motor01dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor02dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor03dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor04dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor05dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor06dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor07dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor08dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor09dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor10dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor11dqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor12dqSignal.getDescriptor(), sampleCount));

    /*packets.push_back(DataPacketWithDomain(domainPacket, motor01ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor02ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor03ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor04ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor05ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor06ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor07ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor08ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor09ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor10ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor11ddqSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor12ddqSignal.getDescriptor(), sampleCount));*/

    packets.push_back(DataPacketWithDomain(domainPacket, motor01tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor02tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor03tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor04tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor05tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor06tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor07tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor08tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor09tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor10tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor11tauSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor12tauSignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, motor01tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor02tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor03tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor04tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor05tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor06tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor07tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor08tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor09tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor10tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor11tempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motor12tempSignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, IMUrSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUpSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUySignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, IMUgxSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUgySignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUgzSignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, IMUaxSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUaySignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUazSignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, IMUquaternionWSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUquaternionXSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUquaternionYSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUquaternionZSignal.getDescriptor(), sampleCount));

    packets.push_back(DataPacketWithDomain(domainPacket, motherboardCurrentSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, motherboardVoltageSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, CPUtempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, IMUtempSignal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, fanFrequency1Signal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, fanFrequency2Signal.getDescriptor(), sampleCount));
    packets.push_back(DataPacketWithDomain(domainPacket, fanFrequency3Signal.getDescriptor(), sampleCount));

    // NOTE: If this is too slow, we can optimize the data structure
    for (size_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
    {
        /*for (size_t i = 0; i < 4; ++i)
        {
            int16_t* packetData = static_cast<int16_t*>(packets[i].getRawData());
            packetData[sampleIndex] = data[4 * sampleIndex + i];
        }*/
        for (size_t i = 0; i < channelCount; ++i)
        {
            int16_t* packetDataF =
                static_cast<int16_t*>(packets[i].getRawData());

            packetDataF[0] = data.forces[i];
        }
        uint8_t* packetDataB = static_cast<uint8_t*>(packets[4].getRawData());
        packetDataB[0] = data.battery;
        for (size_t i = 0; i < 12; ++i)
        {
            float_t* packetDataMq = static_cast<float_t*>(packets[i+5].getRawData());
            packetDataMq[0] = data.motors[i].q();
        }
        for (size_t i = 0; i < 12; ++i)
        {
            float_t* packetDataMdq = static_cast<float_t*>(packets[i+17].getRawData());
            packetDataMdq[0] = data.motors[i].dq();
        }
        /*for (size_t i = 0; i < 12; ++i)
        {
            float_t* packetDataMddq = static_cast<float_t*>(packets[i+29].getRawData());
            packetDataMddq[0] = data.motors[i].ddq();
        }*/
        for (size_t i = 0; i < 12; ++i)
        {
            float_t* packetDataMtau = static_cast<float_t*>(packets[i+29].getRawData());
            packetDataMtau[0] = data.motors[i].tau_est();
        }
        for (size_t i = 0; i < 12; ++i)
        {
            // Descriptor is UInt8 (1 byte per sample): writing through float_t* overflowed the packet buffer
            uint8_t* packetDataMtemp = static_cast<uint8_t*>(packets[i+41].getRawData());
            packetDataMtemp[0] = data.motors[i].temperature();
        }
        for (size_t i = 0; i < 3; ++i)
        {
            float_t* packetDataI = static_cast<float_t*>(packets[i+53].getRawData());
            packetDataI[0] = data.IMU.rpy()[i];
        }
        for (size_t i = 0; i < 3; ++i)
        {
            float_t* packetDataI = static_cast<float_t*>(packets[i+56].getRawData());
            packetDataI[0] = data.IMU.gyroscope()[i];
        }
        for (size_t i = 0; i < 3; ++i)
        {
            float_t* packetDataI = static_cast<float_t*>(packets[i+59].getRawData());
            packetDataI[0] = data.IMU.accelerometer()[i];
        }
        for (size_t i = 0; i < 4; ++i)
        {
            float_t* packetDataQ = static_cast<float_t*>(packets[i+62].getRawData());
            packetDataQ[0] = data.IMU.quaternion()[i];
        }
        // Descriptor is Float32: writing int32_t bits here showed up as garbage (e.g. 1 A -> 1.4e-45)
        float_t* packetDataC = static_cast<float_t*>(packets[66].getRawData());
        packetDataC[0] = data.current;
        float_t* packetDataV = static_cast<float_t*>(packets[67].getRawData());
        packetDataV[0] = data.voltage;
        uint8_t* packetDataCT = static_cast<uint8_t*>(packets[68].getRawData());
        packetDataCT[0] = data.CPUtemp;
        uint8_t* packetDataIT = static_cast<uint8_t*>(packets[69].getRawData());
        packetDataIT[0] = data.IMU.temperature();
        for (size_t i = 0; i < 3; ++i)
        {
            uint16_t* packetDataFreq = static_cast<uint16_t*>(packets[i+70].getRawData());
            packetDataFreq[0] = data.fanFrequency[i];
        }
        
    }

    timeSignal.sendPacket(domainPacket);
    forceFRsignal.sendPacket(packets[0]);
    forceFLsignal.sendPacket(packets[1]);
    forceRRsignal.sendPacket(packets[2]);
    forceRLsignal.sendPacket(packets[3]);
    batterySignal.sendPacket(packets[4]);

    motor01qSignal.sendPacket(packets[5]);
    motor02qSignal.sendPacket(packets[6]);
    motor03qSignal.sendPacket(packets[7]);
    motor04qSignal.sendPacket(packets[8]);
    motor05qSignal.sendPacket(packets[9]);
    motor06qSignal.sendPacket(packets[10]);
    motor07qSignal.sendPacket(packets[11]);
    motor08qSignal.sendPacket(packets[12]);
    motor09qSignal.sendPacket(packets[13]);
    motor10qSignal.sendPacket(packets[14]);
    motor11qSignal.sendPacket(packets[15]);
    motor12qSignal.sendPacket(packets[16]);

    motor01dqSignal.sendPacket(packets[17]);
    motor02dqSignal.sendPacket(packets[18]);
    motor03dqSignal.sendPacket(packets[19]);
    motor04dqSignal.sendPacket(packets[20]);
    motor05dqSignal.sendPacket(packets[21]);
    motor06dqSignal.sendPacket(packets[22]);
    motor07dqSignal.sendPacket(packets[23]);
    motor08dqSignal.sendPacket(packets[24]);
    motor09dqSignal.sendPacket(packets[25]);
    motor10dqSignal.sendPacket(packets[26]);
    motor11dqSignal.sendPacket(packets[27]);
    motor12dqSignal.sendPacket(packets[28]);

    /*motor01ddqSignal.sendPacket(packets[29]);
    motor02ddqSignal.sendPacket(packets[30]);
    motor03ddqSignal.sendPacket(packets[31]);
    motor04ddqSignal.sendPacket(packets[32]);
    motor05ddqSignal.sendPacket(packets[33]);
    motor06ddqSignal.sendPacket(packets[34]);
    motor07ddqSignal.sendPacket(packets[35]);
    motor08ddqSignal.sendPacket(packets[36]);
    motor09ddqSignal.sendPacket(packets[37]);
    motor10ddqSignal.sendPacket(packets[38]);
    motor11ddqSignal.sendPacket(packets[39]);
    motor12ddqSignal.sendPacket(packets[40]);*/

    motor01tauSignal.sendPacket(packets[29]);
    motor02tauSignal.sendPacket(packets[30]);
    motor03tauSignal.sendPacket(packets[31]);
    motor04tauSignal.sendPacket(packets[32]);
    motor05tauSignal.sendPacket(packets[33]);
    motor06tauSignal.sendPacket(packets[34]);
    motor07tauSignal.sendPacket(packets[35]);
    motor08tauSignal.sendPacket(packets[36]);
    motor09tauSignal.sendPacket(packets[37]);
    motor10tauSignal.sendPacket(packets[38]);
    motor11tauSignal.sendPacket(packets[39]);
    motor12tauSignal.sendPacket(packets[40]);

    motor01tempSignal.sendPacket(packets[41]);
    motor02tempSignal.sendPacket(packets[42]);
    motor03tempSignal.sendPacket(packets[43]);
    motor04tempSignal.sendPacket(packets[44]);
    motor05tempSignal.sendPacket(packets[45]);
    motor06tempSignal.sendPacket(packets[46]);
    motor07tempSignal.sendPacket(packets[47]);
    motor08tempSignal.sendPacket(packets[48]);
    motor09tempSignal.sendPacket(packets[49]);
    motor10tempSignal.sendPacket(packets[50]);
    motor11tempSignal.sendPacket(packets[51]);
    motor12tempSignal.sendPacket(packets[52]);

    IMUrSignal.sendPacket(packets[53]);
    IMUpSignal.sendPacket(packets[54]);
    IMUySignal.sendPacket(packets[55]);

    IMUgxSignal.sendPacket(packets[56]);
    IMUgySignal.sendPacket(packets[57]);
    IMUgzSignal.sendPacket(packets[58]);

    IMUaxSignal.sendPacket(packets[59]);
    IMUaySignal.sendPacket(packets[60]);
    IMUazSignal.sendPacket(packets[61]);

    IMUquaternionWSignal.sendPacket(packets[62]);
    IMUquaternionXSignal.sendPacket(packets[63]);
    IMUquaternionYSignal.sendPacket(packets[64]);
    IMUquaternionZSignal.sendPacket(packets[65]);

    motherboardCurrentSignal.sendPacket(packets[66]);
    motherboardVoltageSignal.sendPacket(packets[67]);
    CPUtempSignal.sendPacket(packets[68]);
    IMUtempSignal.sendPacket(packets[69]);
    fanFrequency1Signal.sendPacket(packets[70]);
    fanFrequency2Signal.sendPacket(packets[71]);
    fanFrequency3Signal.sendPacket(packets[72]);
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
    forceFLsignal.setDescriptor(valueDescriptorForce.build());
    forceFRsignal.setDescriptor(valueDescriptorForce.build());
    forceRLsignal.setDescriptor(valueDescriptorForce.build());
    forceRRsignal.setDescriptor(valueDescriptorForce.build());
    const auto valueDescriptorBattery = DataDescriptorBuilder().setSampleType(SampleType::UInt8).setUnit(Unit("%", -1));
    batterySignal.setDescriptor(valueDescriptorBattery.build());
    const auto valueDescriptorRPY = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("rad", -1));
    IMUrSignal.setDescriptor(valueDescriptorRPY.build());
    IMUpSignal.setDescriptor(valueDescriptorRPY.build());
    IMUySignal.setDescriptor(valueDescriptorRPY.build());
    // const auto valueDescriptorQuaternion = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("", -1));
    const auto valueDescriptorGyroscope = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("rad/s", -1));
    IMUgxSignal.setDescriptor(valueDescriptorGyroscope.build());
    IMUgySignal.setDescriptor(valueDescriptorGyroscope.build());
    IMUgzSignal.setDescriptor(valueDescriptorGyroscope.build());
    const auto valueDescriptorAccelerometer = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("m/s^2", -1));
    IMUaxSignal.setDescriptor(valueDescriptorAccelerometer.build());
    IMUaySignal.setDescriptor(valueDescriptorAccelerometer.build());
    IMUazSignal.setDescriptor(valueDescriptorAccelerometer.build());
    const auto valueDescriptorQuaternion = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("", -1));
    IMUquaternionWSignal.setDescriptor(valueDescriptorQuaternion.build());
    IMUquaternionXSignal.setDescriptor(valueDescriptorQuaternion.build());
    IMUquaternionYSignal.setDescriptor(valueDescriptorQuaternion.build());
    IMUquaternionZSignal.setDescriptor(valueDescriptorQuaternion.build());
    
    const auto valueDescriptorCurrent = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("A", -1));
    motherboardCurrentSignal.setDescriptor(valueDescriptorCurrent.build());
    const auto valueDescriptorVoltage = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("V", -1));
    motherboardVoltageSignal.setDescriptor(valueDescriptorVoltage.build());
    const auto valueDescriptorTemperature = DataDescriptorBuilder().setSampleType(SampleType::UInt8).setUnit(Unit("°C", -1));
    CPUtempSignal.setDescriptor(valueDescriptorTemperature.build());
    IMUtempSignal.setDescriptor(valueDescriptorTemperature.build());
    const auto valueDescriptorFrequency = DataDescriptorBuilder().setSampleType(SampleType::UInt16).setUnit(Unit("Hz", -1));
    fanFrequency1Signal.setDescriptor(valueDescriptorFrequency.build());
    fanFrequency2Signal.setDescriptor(valueDescriptorFrequency.build());
    fanFrequency3Signal.setDescriptor(valueDescriptorFrequency.build());

    UnitreeChannel::buildSignalDescriptors_motors();

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
    // std::cout << "Hello from UnitreeChannel::createSignals\n";

    timeSignal = createAndAddSignal(fmt::format("Dog_Time"), nullptr, true);

    forceFLsignal = createAndAddSignal(fmt::format("FL_force"));
    forceFLsignal.setDomainSignal(timeSignal);
    forceFRsignal = createAndAddSignal(fmt::format("FR_force"));
    forceFRsignal.setDomainSignal(timeSignal);
    forceRLsignal = createAndAddSignal(fmt::format("RL_force"));
    forceRLsignal.setDomainSignal(timeSignal);
    forceRRsignal = createAndAddSignal(fmt::format("RR_force"));
    forceRRsignal.setDomainSignal(timeSignal);

    batterySignal = createAndAddSignal(fmt::format("battery"));
    batterySignal.setDomainSignal(timeSignal);

    IMUrSignal = createAndAddSignal(fmt::format("roll"));
    IMUrSignal.setDomainSignal(timeSignal);
    IMUpSignal = createAndAddSignal(fmt::format("pitch"));
    IMUpSignal.setDomainSignal(timeSignal);
    IMUySignal = createAndAddSignal(fmt::format("yaw"));
    IMUySignal.setDomainSignal(timeSignal);

    IMUaxSignal = createAndAddSignal(fmt::format("accelerometer_X"));
    IMUaxSignal.setDomainSignal(timeSignal);
    IMUaySignal = createAndAddSignal(fmt::format("accelerometer_Y"));
    IMUaySignal.setDomainSignal(timeSignal);
    IMUazSignal = createAndAddSignal(fmt::format("accelerometer_Z"));
    IMUazSignal.setDomainSignal(timeSignal);

    IMUgxSignal = createAndAddSignal(fmt::format("gyroscope_X"));
    IMUgxSignal.setDomainSignal(timeSignal);
    IMUgySignal = createAndAddSignal(fmt::format("gyroscope_Y"));
    IMUgySignal.setDomainSignal(timeSignal);
    IMUgzSignal = createAndAddSignal(fmt::format("gyroscope_Z"));
    IMUgzSignal.setDomainSignal(timeSignal);

    IMUquaternionWSignal = createAndAddSignal(fmt::format("quaternion_W"));
    IMUquaternionWSignal.setDomainSignal(timeSignal);
    IMUquaternionXSignal = createAndAddSignal(fmt::format("quaternion_X"));
    IMUquaternionXSignal.setDomainSignal(timeSignal);
    IMUquaternionYSignal = createAndAddSignal(fmt::format("quaternion_Y"));
    IMUquaternionYSignal.setDomainSignal(timeSignal);
    IMUquaternionZSignal = createAndAddSignal(fmt::format("quaternion_Z"));
    IMUquaternionZSignal.setDomainSignal(timeSignal);

    motherboardCurrentSignal = createAndAddSignal(fmt::format("current"));
    motherboardCurrentSignal.setDomainSignal(timeSignal);
    motherboardVoltageSignal = createAndAddSignal(fmt::format("voltage"));
    motherboardVoltageSignal.setDomainSignal(timeSignal);
    CPUtempSignal = createAndAddSignal(fmt::format("CPU_temperature"));
    CPUtempSignal.setDomainSignal(timeSignal);
    IMUtempSignal = createAndAddSignal(fmt::format("IMU_temperature"));
    IMUtempSignal.setDomainSignal(timeSignal);
    fanFrequency1Signal = createAndAddSignal(fmt::format("LR_fan_frequency"));
    fanFrequency1Signal.setDomainSignal(timeSignal);
    fanFrequency2Signal = createAndAddSignal(fmt::format("RR_fan_frequency"));
    fanFrequency2Signal.setDomainSignal(timeSignal);
    fanFrequency3Signal = createAndAddSignal(fmt::format("front_fan_frequency"));
    fanFrequency3Signal.setDomainSignal(timeSignal);

    UnitreeChannel::createSignals_motors();
}

void UnitreeChannel::createSignals_motors()
{
    motor01qSignal = createAndAddSignal(fmt::format("motorFR1_q"));
    motor02qSignal = createAndAddSignal(fmt::format("motorFR2_q"));
    motor03qSignal = createAndAddSignal(fmt::format("motorFR3_q"));
    motor04qSignal = createAndAddSignal(fmt::format("motorFL1_q"));
    motor05qSignal = createAndAddSignal(fmt::format("motorFL2_q"));
    motor06qSignal = createAndAddSignal(fmt::format("motorFL3_q"));
    motor07qSignal = createAndAddSignal(fmt::format("motorRR1_q"));
    motor08qSignal = createAndAddSignal(fmt::format("motorRR2_q"));
    motor09qSignal = createAndAddSignal(fmt::format("motorRR3_q"));
    motor10qSignal = createAndAddSignal(fmt::format("motorRL1_q"));
    motor11qSignal = createAndAddSignal(fmt::format("motorRL2_q"));
    motor12qSignal = createAndAddSignal(fmt::format("motorRL3_q"));
    motor01qSignal.setDomainSignal(timeSignal);
    motor02qSignal.setDomainSignal(timeSignal);
    motor03qSignal.setDomainSignal(timeSignal);
    motor04qSignal.setDomainSignal(timeSignal);
    motor05qSignal.setDomainSignal(timeSignal);
    motor06qSignal.setDomainSignal(timeSignal);
    motor07qSignal.setDomainSignal(timeSignal);
    motor08qSignal.setDomainSignal(timeSignal);
    motor09qSignal.setDomainSignal(timeSignal);
    motor10qSignal.setDomainSignal(timeSignal);
    motor11qSignal.setDomainSignal(timeSignal);
    motor12qSignal.setDomainSignal(timeSignal);

    motor01dqSignal = createAndAddSignal(fmt::format("motorFR1_dq"));
    motor02dqSignal = createAndAddSignal(fmt::format("motorFR2_dq"));
    motor03dqSignal = createAndAddSignal(fmt::format("motorFR3_dq"));
    motor04dqSignal = createAndAddSignal(fmt::format("motorFL1_dq"));
    motor05dqSignal = createAndAddSignal(fmt::format("motorFL2_dq"));
    motor06dqSignal = createAndAddSignal(fmt::format("motorFL3_dq"));
    motor07dqSignal = createAndAddSignal(fmt::format("motorRR1_dq"));
    motor08dqSignal = createAndAddSignal(fmt::format("motorRR2_dq"));
    motor09dqSignal = createAndAddSignal(fmt::format("motorRR3_dq"));
    motor10dqSignal = createAndAddSignal(fmt::format("motorRL1_dq"));
    motor11dqSignal = createAndAddSignal(fmt::format("motorRL2_dq"));
    motor12dqSignal = createAndAddSignal(fmt::format("motorRL3_dq"));
    motor01dqSignal.setDomainSignal(timeSignal);
    motor02dqSignal.setDomainSignal(timeSignal);
    motor03dqSignal.setDomainSignal(timeSignal);
    motor04dqSignal.setDomainSignal(timeSignal);
    motor05dqSignal.setDomainSignal(timeSignal);
    motor06dqSignal.setDomainSignal(timeSignal);
    motor07dqSignal.setDomainSignal(timeSignal);
    motor08dqSignal.setDomainSignal(timeSignal);
    motor09dqSignal.setDomainSignal(timeSignal);
    motor10dqSignal.setDomainSignal(timeSignal);
    motor11dqSignal.setDomainSignal(timeSignal);
    motor12dqSignal.setDomainSignal(timeSignal);

    /*motor01ddqSignal = createAndAddSignal(fmt::format("motorFR1_ddq"));
    motor02ddqSignal = createAndAddSignal(fmt::format("motorFR2_ddq"));
    motor03ddqSignal = createAndAddSignal(fmt::format("motorFR3_ddq"));
    motor04ddqSignal = createAndAddSignal(fmt::format("motorFL1_ddq"));
    motor05ddqSignal = createAndAddSignal(fmt::format("motorFL2_ddq"));
    motor06ddqSignal = createAndAddSignal(fmt::format("motorFL3_ddq"));
    motor07ddqSignal = createAndAddSignal(fmt::format("motorRR1_ddq"));
    motor08ddqSignal = createAndAddSignal(fmt::format("motorRR2_ddq"));
    motor09ddqSignal = createAndAddSignal(fmt::format("motorRR3_ddq"));
    motor10ddqSignal = createAndAddSignal(fmt::format("motorRL1_ddq"));
    motor11ddqSignal = createAndAddSignal(fmt::format("motorRL2_ddq"));
    motor12ddqSignal = createAndAddSignal(fmt::format("motorRL3_ddq"));
    motor01ddqSignal.setDomainSignal(timeSignal);
    motor02ddqSignal.setDomainSignal(timeSignal);
    motor03ddqSignal.setDomainSignal(timeSignal);
    motor04ddqSignal.setDomainSignal(timeSignal);
    motor05ddqSignal.setDomainSignal(timeSignal);
    motor06ddqSignal.setDomainSignal(timeSignal);
    motor07ddqSignal.setDomainSignal(timeSignal);
    motor08ddqSignal.setDomainSignal(timeSignal);
    motor09ddqSignal.setDomainSignal(timeSignal);
    motor10ddqSignal.setDomainSignal(timeSignal);
    motor11ddqSignal.setDomainSignal(timeSignal);
    motor12ddqSignal.setDomainSignal(timeSignal);*/

    motor01tauSignal = createAndAddSignal(fmt::format("motorFR1_tau"));
    motor02tauSignal = createAndAddSignal(fmt::format("motorFR2_tau"));
    motor03tauSignal = createAndAddSignal(fmt::format("motorFR3_tau"));
    motor04tauSignal = createAndAddSignal(fmt::format("motorFL1_tau"));
    motor05tauSignal = createAndAddSignal(fmt::format("motorFL2_tau"));
    motor06tauSignal = createAndAddSignal(fmt::format("motorFL3_tau"));
    motor07tauSignal = createAndAddSignal(fmt::format("motorRR1_tau"));
    motor08tauSignal = createAndAddSignal(fmt::format("motorRR2_tau"));
    motor09tauSignal = createAndAddSignal(fmt::format("motorRR3_tau"));
    motor10tauSignal = createAndAddSignal(fmt::format("motorRL1_tau"));
    motor11tauSignal = createAndAddSignal(fmt::format("motorRL2_tau"));
    motor12tauSignal = createAndAddSignal(fmt::format("motorRL3_tau"));
    motor01tauSignal.setDomainSignal(timeSignal);
    motor02tauSignal.setDomainSignal(timeSignal);
    motor03tauSignal.setDomainSignal(timeSignal);
    motor04tauSignal.setDomainSignal(timeSignal);
    motor05tauSignal.setDomainSignal(timeSignal);
    motor06tauSignal.setDomainSignal(timeSignal);
    motor07tauSignal.setDomainSignal(timeSignal);
    motor08tauSignal.setDomainSignal(timeSignal);
    motor09tauSignal.setDomainSignal(timeSignal);
    motor10tauSignal.setDomainSignal(timeSignal);
    motor11tauSignal.setDomainSignal(timeSignal);
    motor12tauSignal.setDomainSignal(timeSignal);

    motor01tempSignal = createAndAddSignal(fmt::format("motorFR1_temperature"));
    motor02tempSignal = createAndAddSignal(fmt::format("motorFR2_temperature"));
    motor03tempSignal = createAndAddSignal(fmt::format("motorFR3_temperature"));
    motor04tempSignal = createAndAddSignal(fmt::format("motorFL1_temperature"));
    motor05tempSignal = createAndAddSignal(fmt::format("motorFL2_temperature"));
    motor06tempSignal = createAndAddSignal(fmt::format("motorFL3_temperature"));
    motor07tempSignal = createAndAddSignal(fmt::format("motorRR1_temperature"));
    motor08tempSignal = createAndAddSignal(fmt::format("motorRR2_temperature"));
    motor09tempSignal = createAndAddSignal(fmt::format("motorRR3_temperature"));
    motor10tempSignal = createAndAddSignal(fmt::format("motorRL1_temperature"));
    motor11tempSignal = createAndAddSignal(fmt::format("motorRL2_temperature"));
    motor12tempSignal = createAndAddSignal(fmt::format("motorRL3_temperature"));
    motor01tempSignal.setDomainSignal(timeSignal);
    motor02tempSignal.setDomainSignal(timeSignal);
    motor03tempSignal.setDomainSignal(timeSignal);
    motor04tempSignal.setDomainSignal(timeSignal);
    motor05tempSignal.setDomainSignal(timeSignal);
    motor06tempSignal.setDomainSignal(timeSignal);
    motor07tempSignal.setDomainSignal(timeSignal);
    motor08tempSignal.setDomainSignal(timeSignal);
    motor09tempSignal.setDomainSignal(timeSignal);
    motor10tempSignal.setDomainSignal(timeSignal);
    motor11tempSignal.setDomainSignal(timeSignal);
    motor12tempSignal.setDomainSignal(timeSignal);
}

void UnitreeChannel::buildSignalDescriptors_motors()
{
    const auto valueDescriptorMq = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("rad", -1));
    const auto valueDescriptorMdq = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("", -1));
    // const auto valueDescriptorMddq = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("", -1));
    const auto valueDescriptorMtau = DataDescriptorBuilder().setSampleType(SampleType::Float32).setUnit(Unit("", -1));
    const auto valueDescriptorMtemp = DataDescriptorBuilder().setSampleType(SampleType::UInt8).setUnit(Unit("°C", -1));
    
    motor01qSignal.setDescriptor(valueDescriptorMq.build());
    motor02qSignal.setDescriptor(valueDescriptorMq.build());
    motor03qSignal.setDescriptor(valueDescriptorMq.build());
    motor04qSignal.setDescriptor(valueDescriptorMq.build());
    motor05qSignal.setDescriptor(valueDescriptorMq.build());
    motor06qSignal.setDescriptor(valueDescriptorMq.build());
    motor07qSignal.setDescriptor(valueDescriptorMq.build());
    motor08qSignal.setDescriptor(valueDescriptorMq.build());
    motor09qSignal.setDescriptor(valueDescriptorMq.build());
    motor10qSignal.setDescriptor(valueDescriptorMq.build());
    motor11qSignal.setDescriptor(valueDescriptorMq.build());
    motor12qSignal.setDescriptor(valueDescriptorMq.build());

    motor01dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor02dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor03dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor04dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor05dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor06dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor07dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor08dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor09dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor10dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor11dqSignal.setDescriptor(valueDescriptorMdq.build());
    motor12dqSignal.setDescriptor(valueDescriptorMdq.build());

    /*motor01ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor02ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor03ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor04ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor05ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor06ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor07ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor08ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor09ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor10ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor11ddqSignal.setDescriptor(valueDescriptorMddq.build());
    motor12ddqSignal.setDescriptor(valueDescriptorMddq.build());*/

    motor01tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor02tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor03tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor04tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor05tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor06tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor07tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor08tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor09tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor10tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor11tauSignal.setDescriptor(valueDescriptorMtau.build());
    motor12tauSignal.setDescriptor(valueDescriptorMtau.build());

    motor01tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor02tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor03tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor04tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor05tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor06tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor07tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor08tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor09tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor10tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor11tempSignal.setDescriptor(valueDescriptorMtemp.build());
    motor12tempSignal.setDescriptor(valueDescriptorMtemp.build());
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