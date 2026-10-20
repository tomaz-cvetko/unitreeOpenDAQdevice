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
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <opendaq/channel_ptr.h>
#include <opendaq/device_impl.h>

#include <unitree_module/common.h>
#include <unitree_module/sensor_data_listener.h>

BEGIN_NAMESPACE_UNITREE_MODULE

class UnitreeDevice final : public Device
{
public:
    explicit UnitreeDevice(const ContextPtr& ctx, const ComponentPtr& parent);
    ~UnitreeDevice() override;

    static DeviceInfoPtr CreateDeviceInfo();
    static DeviceTypePtr CreateType();

    // IDevice
    DeviceInfoPtr onGetInfo() override;
    uint64_t onGetTicksSinceOrigin() override;

private:
    void init();
    void stopDds();
    void stopAcqThread();
    void initDomain();
    void initChannels();

    void acqLoop();
    // Called on the DDS listener thread: only queues the sample, all openDAQ work happens in acqLoop
    void forwardDataCallback(SensorDataListenerLowState::BufferS&& data);

    std::chrono::microseconds getMicroSecondsSinceDeviceStart() const;

    bool stopAcq;
    std::deque<SensorDataListenerLowState::BufferS> queue;  // bounded, see MaxQueuedSamples
    uint64_t droppedQueueFull = 0;  // guarded by mutex

    std::mutex mutex;
    std::condition_variable cv;
    std::thread acqThread;

    std::chrono::steady_clock::time_point startTime;
    std::chrono::microseconds microSecondsFromEpochToDeviceStart;

    ChannelPtr channel1;

    std::unique_ptr<SensorDataListenerLowState> low_state_listener;
    std::optional<dds::sub::DataReader<SensorDataListenerLowState::MsgType>> reader;

};

END_NAMESPACE_UNITREE_MODULE
