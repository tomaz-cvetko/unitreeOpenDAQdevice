#include <unitree_module/common.h>

/* Include the C++ DDS API. */
#include "dds/dds.hpp"
#include "dds/domain/qos/DomainParticipantQos.hpp"

#include "CycloneData.hpp"

BEGIN_NAMESPACE_UNITREE_MODULE

namespace
{
    using MsgType_ = CycloneData::Msg;
}

class SensorDataListener : public virtual dds::sub::DataReaderListener<MsgType_>
{
public:
    // using MsgType = unitree_go::msg::dds_::LowState_;
    using MsgType = MsgType_;
    using ArgType = std::vector<int16_t>&&;
    inline static std::string GetTopicName()
    {
        return "CycloneData_Msg";
    }

    explicit SensorDataListener(const std::function<void(std::vector<int16_t>&&)>& func);

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
    std::function<void(std::vector<int16_t>&&)> callback;
};

END_NAMESPACE_UNITREE_MODULE