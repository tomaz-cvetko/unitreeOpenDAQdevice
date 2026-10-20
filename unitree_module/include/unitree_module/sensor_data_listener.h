#pragma once
#include <unitree_module/common.h>

/* Include the C++ DDS API. */
#include "dds/dds.hpp"
#include "dds/domain/qos/DomainParticipantQos.hpp"

#include "unitree_module/LowState.hpp"
#include "unitree_module/IMUState.hpp"
#include "unitree_module/MotorState.hpp"

#include <array>
#include <chrono>
#include <functional>
#include <string>

BEGIN_NAMESPACE_UNITREE_MODULE

class SensorDataListenerLowState : public virtual dds::sub::DataReaderListener<unitree_go::msg::dds_::LowState_>
{
public:
    using MsgType = unitree_go::msg::dds_::LowState_;
    inline static std::string GetTopicName()
    {
        return "rt/lowstate";
    }

    struct BufferS
    {
        std::array<int16_t, 4> forces;
        uint8_t battery;
        unitree_go::msg::dds_::IMUState_ IMU;
        std::array<unitree_go::msg::dds_::MotorState_, 12> motors;
        float_t current;  // LowState::power_a is float
        float_t voltage;
        uint8_t CPUtemp;
        std::array<uint16_t, 4> fanFrequency;
        uint32_t tick;  // LowState::tick: the dog's own clock, 1 ms steps (incremented by 2 per 500 Hz message)
        std::chrono::microseconds arrivalTime{0};  // PC time (since device start) when the sample was received
    };

    explicit SensorDataListenerLowState(std::function<void(BufferS&&)> func);

    void on_data_available(dds::sub::DataReader<MsgType>& reader) override;

    void on_subscription_matched(dds::sub::DataReader<MsgType>& reader,
                                 const dds::core::status::SubscriptionMatchedStatus& status) override;

    void on_sample_lost(dds::sub::DataReader<MsgType>& reader, const dds::core::status::SampleLostStatus& status) override;

    void on_requested_deadline_missed(dds::sub::DataReader<MsgType>& reader,
                                      const dds::core::status::RequestedDeadlineMissedStatus& status) override;

    void on_requested_incompatible_qos(dds::sub::DataReader<MsgType>& reader,
                                       const dds::core::status::RequestedIncompatibleQosStatus& status) override;

    void on_sample_rejected(dds::sub::DataReader<MsgType>& reader, const dds::core::status::SampleRejectedStatus& status) override;

    void on_liveliness_changed(dds::sub::DataReader<MsgType>& reader, const dds::core::status::LivelinessChangedStatus& status) override;

private:
    std::function<void(BufferS&&)> callback;
};

END_NAMESPACE_UNITREE_MODULE
