#pragma once
#include <unitree_module/common.h>

/* Include the C++ DDS API. */
#include "dds/dds.hpp"
#include "dds/domain/qos/DomainParticipantQos.hpp"

// #include "CycloneData.hpp"
#include "unitree_module/LowState.hpp"
#include "unitree_module/IMUState.hpp"
#include "unitree_module/MotorState.hpp"
// #include "unitree_module/SportModeState.hpp"

BEGIN_NAMESPACE_UNITREE_MODULE

namespace
{
    // using MsgType_ = CycloneData::Msg;
    using MsgTypeLS_ = unitree_go::msg::dds_::LowState_;
    // using MsgTypeSMS_ = unitree_go::msg::dds_::SportModeState_;
}

class SensorDataListenerLowState : public virtual dds::sub::DataReaderListener<MsgTypeLS_>
{
public:
    // using MsgType = unitree_go::msg::dds_::LowState_;
    using MsgType = MsgTypeLS_;
    using ArgType = std::vector<int16_t>&&;
    // using ArgType = std::vector<float_t>&&;
    inline static std::string GetTopicName()
    {
        // return "CycloneData_Msg";
        return "rt/lowstate";
    }

    struct BufferS
    {
        // std::vector<int16_t> forces{4};
        std::array<int16_t, 4> forces;
        uint8_t battery;
        unitree_go::msg::dds_::IMUState_ IMU;
        std::array<unitree_go::msg::dds_::MotorState_, 12> motors;
        float_t current;  // LowState::power_a is float
        float_t voltage;
        uint8_t CPUtemp;
        std::array<uint16_t, 4> fanFrequency;
    };

    // auto now;
    // auto duration;
    // auto milliseconds;

    bool printOnce;

    // explicit SensorDataListenerLowState(const std::function<void(BufferS&&)>& func);
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
    // std::function<void(std::vector<int16_t>&&)> callback;
    // std::function<void(std::vector<float_t>&&)> callback;
    std::function<void(BufferS&&)> callback;
};

/*class SensorDataListenerSportModeState : public virtual dds::sub::DataReaderListener<MsgTypeSMS_>
{
public:
    using MsgTypeSMS = MsgTypeSMS_;
    using ArgTypeSMS = std::vector<int16_t>&&;
    inline static std::string GetTopicName()
    {
        return "rt/sportmodestate";
    }
};*/

END_NAMESPACE_UNITREE_MODULE