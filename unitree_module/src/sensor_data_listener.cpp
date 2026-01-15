#include <unitree_module/sensor_data_listener.h>

BEGIN_NAMESPACE_UNITREE_MODULE

SensorDataListener::SensorDataListener(const std::function<void(std::vector<int16_t>&&)>& func)
    : callback(func)
{
    std::cout << "Hello from SensorDataListener!" << std::endl;
}

void SensorDataListener::on_data_available(dds::sub::DataReader<MsgType>& reader)
{
    std::cout << "data available" << std::endl;
    dds::sub::LoanedSamples<MsgType> samples = reader.take();
    size_t numberOfSamples = samples.length();

    constexpr size_t numberOfComponents = 4;
    std::vector<int16_t> buffer(numberOfComponents * numberOfSamples, 0);

    size_t sampleIndex = 0;
    for (const auto& sample : samples)
    {
        if (!sample.info().valid())
        {
            continue;
        }

        const MsgType& msg = sample.data();
        const std::array<int16_t, 4>& values = msg.foot_force();

        // Pack into flat array as [x0, y0, z0, w0, x1, y1, z1, w1]
        std::copy(values.data(), values.data() + 4, &buffer[4 * sampleIndex]);

        // Equivalent but a bit easer to control
        // for (size_t i = 0; i < 4; ++i)
        // {
        //     const size_t index = numberOfComponents * sampleIndex + i;
        //     buffer[i] - values[i];
        // }

        // Question: Is there any information about the time these values were measured that we must propagate?

        ++sampleIndex;
    }
    this->callback(std::move(buffer));
}

void SensorDataListener::on_subscription_matched(dds::sub::DataReader<MsgType>& reader,
                                                 const dds::core::status::SubscriptionMatchedStatus& status)
{
    std::cout << "on_subscription_matched" << std::endl;
}

void SensorDataListener::on_sample_lost(dds::sub::DataReader<MsgType>& reader, const dds::core::status::SampleLostStatus& status)
{
    std::cout << "on_sample_lost" << std::endl;
}

void SensorDataListener::on_requested_deadline_missed(dds::sub::DataReader<MsgType>& reader,
                                                      const dds::core::status::RequestedDeadlineMissedStatus& status)
{
    std::cout << "on_requested_deadline_missed" << std::endl;
}

void SensorDataListener::on_requested_incompatible_qos(dds::sub::DataReader<MsgType>& reader,
                                                       const dds::core::status::RequestedIncompatibleQosStatus& status)
{
    std::cout << "on_requested_incompatible_qos" << std::endl;
}

void SensorDataListener::on_sample_rejected(dds::sub::DataReader<MsgType>& reader, const dds::core::status::SampleRejectedStatus& status)
{
    std::cout << "on_sample_rejected" << std::endl;
}

void SensorDataListener::on_liveliness_changed(dds::sub::DataReader<MsgType>& reader,
                                               const dds::core::status::LivelinessChangedStatus& status)
{
    std::cout << "on_liveliness_changed" << std::endl;
}

END_NAMESPACE_UNITREE_MODULE