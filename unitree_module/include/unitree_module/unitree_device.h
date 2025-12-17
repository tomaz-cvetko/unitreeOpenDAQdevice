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
#include <queue>
#include <thread>

#include <opendaq/channel_ptr.h>
#include <opendaq/device_impl.h>

#include <unitree_module/common.h>

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
    void initDomain();
    void initChannels();

    // void processData(int16_t data) const;
    // void processData(std::vector<int16_t> data) const;

    void acqLoop();
    void forwardDataCallback(std::vector<int16_t>&& data);
    void forwardData(std::vector<int16_t>& data);

    std::chrono::microseconds getMicroSecondsSinceDeviceStart() const;

    bool stopAcq;
    std::queue<std::vector<int16_t>> queue;

    std::mutex mutex;
    std::condition_variable cv;
    std::thread acqThread;

    std::chrono::steady_clock::time_point startTime;
    std::chrono::microseconds microSecondsFromEpochToDeviceStart;

    ChannelPtr channel1;
};

END_NAMESPACE_UNITREE_MODULE
