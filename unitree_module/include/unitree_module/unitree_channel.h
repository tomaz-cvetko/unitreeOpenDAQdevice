/*
 * Copyright 2022-2024 openDAQ d.o.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <opendaq/channel_impl.h>
#include <opendaq/signal_config_ptr.h>
#include <unitree_module/common.h>
#include <optional>
#include <random>
#include <vector>
#include <unitree_module/sensor_data_listener.h>

BEGIN_NAMESPACE_UNITREE_MODULE

enum class WaveformType { Sine, Rect, None, Counter, ConstantValue };

DECLARE_OPENDAQ_INTERFACE(IDogChannel, IBaseObject)
{
    // virtual void publishSamples(std::chrono::microseconds curTime, std::vector<int16_t> & data) = 0;
    virtual void publishSamples(std::chrono::microseconds curTime, SensorDataListenerLowState::BufferS & data) = 0;
};

struct UnitreeChannelInit
{
    size_t index;
    double sampleRate;
    std::chrono::microseconds startTime;
    std::chrono::microseconds microSecondsFromEpochToStartTime;
};

class UnitreeChannel final : public ChannelImpl<IDogChannel>
{
public:
    explicit UnitreeChannel(const ContextPtr& context,
                            const ComponentPtr& parent,
                            const StringPtr& localId,
                            const UnitreeChannelInit& init);

    // IDogChannel
    // void publishSamples(std::chrono::microseconds curTime, std::vector<int16_t>& data) override;
    void publishSamples(std::chrono::microseconds curTime, SensorDataListenerLowState::BufferS& data) override;

    static std::string getEpoch();
    static RatioPtr getResolution();

private:
    void initProperties();
    void createSignals();
    void buildSignalDescriptors();
    void buildSignalDescriptors_motors();
    void createSignals_motors();

    uint64_t getSamplesSinceStart(std::chrono::microseconds time) const;
    [[nodiscard]] Int getDeltaT(const double sr) const;

    uint64_t deltaT;
    size_t index;

    std::chrono::microseconds startTime;
    std::chrono::microseconds microSecondsFromEpochToStartTime;
    std::chrono::microseconds lastCollectTime;
    uint64_t samplesGenerated;

    uint64_t counter;
    double sampleRate;

    uint64_t startTick = 0;

    SignalConfigPtr timeSignal;

    SignalConfigPtr forceFLsignal;
    SignalConfigPtr forceFRsignal;
    SignalConfigPtr forceRLsignal;
    SignalConfigPtr forceRRsignal;
    
    SignalConfigPtr batterySignal;
    
    SignalConfigPtr motor01qSignal;
    SignalConfigPtr motor02qSignal;
    SignalConfigPtr motor03qSignal;
    SignalConfigPtr motor04qSignal;
    SignalConfigPtr motor05qSignal;
    SignalConfigPtr motor06qSignal;
    SignalConfigPtr motor07qSignal;
    SignalConfigPtr motor08qSignal;
    SignalConfigPtr motor09qSignal;
    SignalConfigPtr motor10qSignal;
    SignalConfigPtr motor11qSignal;
    SignalConfigPtr motor12qSignal;

    SignalConfigPtr motor01dqSignal;
    SignalConfigPtr motor02dqSignal;
    SignalConfigPtr motor03dqSignal;
    SignalConfigPtr motor04dqSignal;
    SignalConfigPtr motor05dqSignal;
    SignalConfigPtr motor06dqSignal;
    SignalConfigPtr motor07dqSignal;
    SignalConfigPtr motor08dqSignal;
    SignalConfigPtr motor09dqSignal;
    SignalConfigPtr motor10dqSignal;
    SignalConfigPtr motor11dqSignal;
    SignalConfigPtr motor12dqSignal;

    /*SignalConfigPtr motor01ddqSignal;
    SignalConfigPtr motor02ddqSignal;
    SignalConfigPtr motor03ddqSignal;
    SignalConfigPtr motor04ddqSignal;
    SignalConfigPtr motor05ddqSignal;
    SignalConfigPtr motor06ddqSignal;
    SignalConfigPtr motor07ddqSignal;
    SignalConfigPtr motor08ddqSignal;
    SignalConfigPtr motor09ddqSignal;
    SignalConfigPtr motor10ddqSignal;
    SignalConfigPtr motor11ddqSignal;
    SignalConfigPtr motor12ddqSignal;*/

    SignalConfigPtr motor01tauSignal;
    SignalConfigPtr motor02tauSignal;
    SignalConfigPtr motor03tauSignal;
    SignalConfigPtr motor04tauSignal;
    SignalConfigPtr motor05tauSignal;
    SignalConfigPtr motor06tauSignal;
    SignalConfigPtr motor07tauSignal;
    SignalConfigPtr motor08tauSignal;
    SignalConfigPtr motor09tauSignal;
    SignalConfigPtr motor10tauSignal;
    SignalConfigPtr motor11tauSignal;
    SignalConfigPtr motor12tauSignal;

    SignalConfigPtr motor01tempSignal;
    SignalConfigPtr motor02tempSignal;
    SignalConfigPtr motor03tempSignal;
    SignalConfigPtr motor04tempSignal;
    SignalConfigPtr motor05tempSignal;
    SignalConfigPtr motor06tempSignal;
    SignalConfigPtr motor07tempSignal;
    SignalConfigPtr motor08tempSignal;
    SignalConfigPtr motor09tempSignal;
    SignalConfigPtr motor10tempSignal;
    SignalConfigPtr motor11tempSignal;
    SignalConfigPtr motor12tempSignal;
    
    SignalConfigPtr IMUrSignal;
    SignalConfigPtr IMUpSignal;
    SignalConfigPtr IMUySignal;

    SignalConfigPtr IMUgxSignal;
    SignalConfigPtr IMUgySignal;
    SignalConfigPtr IMUgzSignal;

    SignalConfigPtr IMUaxSignal;
    SignalConfigPtr IMUaySignal;
    SignalConfigPtr IMUazSignal;

    SignalConfigPtr IMUquaternionWSignal;
    SignalConfigPtr IMUquaternionXSignal;
    SignalConfigPtr IMUquaternionYSignal;
    SignalConfigPtr IMUquaternionZSignal;

    SignalConfigPtr motherboardCurrentSignal;
    SignalConfigPtr motherboardVoltageSignal;
    SignalConfigPtr CPUtempSignal;
    SignalConfigPtr IMUtempSignal;
    SignalConfigPtr fanFrequency1Signal;
    SignalConfigPtr fanFrequency2Signal;
    SignalConfigPtr fanFrequency3Signal;

};

END_NAMESPACE_UNITREE_MODULE
