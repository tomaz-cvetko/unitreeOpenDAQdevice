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

BEGIN_NAMESPACE_UNITREE_MODULE

enum class WaveformType { Sine, Rect, None, Counter, ConstantValue };

DECLARE_OPENDAQ_INTERFACE(IDogChannel, IBaseObject)
{
    virtual void publishSamples(std::chrono::microseconds curTime, std::vector<int16_t> & data) = 0;
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
    void publishSamples(std::chrono::microseconds curTime, std::vector<int16_t>& data) override;

    static std::string getEpoch();
    static RatioPtr getResolution();

private:
    void initProperties();
    void createSignals();
    void buildSignalDescriptors();

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

    SignalConfigPtr timeSignal;

    SignalConfigPtr forceFLsignal;
    SignalConfigPtr forceFRsignal;
    SignalConfigPtr forceRLsignal;
    SignalConfigPtr forceRRsignal;
};

END_NAMESPACE_UNITREE_MODULE
