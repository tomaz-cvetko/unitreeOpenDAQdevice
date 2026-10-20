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
#include <opendaq/data_descriptor_ptr.h>
#include <opendaq/signal_config_ptr.h>
#include <unitree_module/common.h>
#include <functional>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <unitree_module/sensor_data_listener.h>

BEGIN_NAMESPACE_UNITREE_MODULE

using DogSample = SensorDataListenerLowState::BufferS;

DECLARE_OPENDAQ_INTERFACE(IDogChannel, IBaseObject)
{
    // Called from the device's acquisition thread with all samples received since the previous call.
    // The list may be empty: the call then only sends out a partly filled packet that has waited long enough.
    virtual void publishSamples(std::chrono::microseconds curTime, const std::vector<DogSample>& samples) = 0;
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
    void publishSamples(std::chrono::microseconds curTime, const std::vector<DogSample>& samples) override;

    static std::string getEpoch();
    static RatioPtr getResolution();

private:
    // One value signal: its descriptor is built from the C++ type T used to write its samples,
    // so the sample type in the descriptor and the bytes written into the packet always match.
    struct ValueSignal
    {
        SignalConfigPtr signal;
        DataDescriptorPtr descriptor;
        std::function<void(void* packetData, size_t sampleIndex, const DogSample& sample)> write;
    };

    void initProperties();
    void createSignals();
    template <typename T, typename Getter>
    void addValueSignal(const std::string& name, const std::string& unit, Getter getter);

    [[nodiscard]] Int getDeltaT(const double sr) const;

    // Domain value (us since epoch) for a sample with the given dog tick; std::nullopt = drop the sample.
    std::optional<uint64_t> getSampleTime(uint32_t dogTick, uint64_t pcTimeUs);
    void anchorTime(uint32_t dogTick, uint64_t pcTimeUs);

    // Samples are collected into blocks and sent as one packet per signal (instead of one packet per sample)
    void appendSample(const DogSample& sample, uint64_t sampleTime, uint64_t pcTimeUs);
    void sendBlock();

    uint64_t deltaT;
    size_t index;

    std::chrono::microseconds startTime;
    std::chrono::microseconds microSecondsFromEpochToStartTime;
    double sampleRate;

    // Time axis from LowState::tick: anchored once to the PC clock, then advanced by the dog's ticks,
    // so lost DDS messages show up as gaps instead of shifting all later timestamps into the past.
    bool timeAnchored = false;
    uint64_t anchorTimeUs = 0;        // domain value of the anchor sample
    uint32_t lastDogTick = 0;
    uint64_t dogTicksSinceAnchor = 0; // 64-bit, so the 32-bit tick wrap-around (after 49.7 days) is harmless
    uint64_t droppedDuplicates = 0;

    // Slow drift between the dog's clock and the PC clock, corrected by repeating or dropping single samples
    // (see trackClockOffset). Offset = arrival time - timestamp: the minimum over a window is the clock offset
    // without network/scheduling delays; it should stay near 0.
    void trackClockOffset(int64_t offsetUs, uint64_t arrivalUs);
    int64_t windowMinOffsetUs = INT64_MAX;
    uint64_t windowStartUs = 0;
    int64_t pendingSlips = 0;         // > 0: repeat samples (timestamps behind), < 0: drop samples (timestamps ahead)
    uint64_t samplesSinceSlip = 0;

    // Block being assembled: equally spaced samples (deltaT apart) starting at blockStartTime
    std::vector<DogSample> block;
    uint64_t blockStartTime = 0;
    uint64_t blockFirstArrivalUs = 0;  // PC time when the first sample of the block arrived

    SignalConfigPtr timeSignal;
    std::vector<ValueSignal> valueSignals;
};

END_NAMESPACE_UNITREE_MODULE
