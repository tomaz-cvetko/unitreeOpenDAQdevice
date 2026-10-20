#include <unitree_module/sensor_data_listener.h>
#include <algorithm>
#include <chrono>
#include <iostream>

BEGIN_NAMESPACE_UNITREE_MODULE

SensorDataListenerLowState::SensorDataListenerLowState(std::function<void(BufferS&&)> func)
    : callback(std::move(func))
{
    std::cout << "Hello from SensorDataListenerLowState!" << std::endl;
}

void SensorDataListenerLowState::on_data_available(dds::sub::DataReader<MsgType>& reader)
{
    dds::sub::LoanedSamples<MsgType> samples = reader.take();

    for (const auto& sample : samples)
    {
        // Invalid samples (e.g. dispose/unregister notifications) carry no data
        if (!sample.info().valid())
        {
            continue;
        }

        const MsgType& msg = sample.data();

        BufferS buffer_s;
        buffer_s.forces = msg.foot_force();
        buffer_s.battery = msg.bms_state().soc();
        buffer_s.IMU = msg.imu_state();
        std::copy_n(msg.motor_state().begin(), 12, buffer_s.motors.begin());
        buffer_s.current = msg.power_a();
        buffer_s.voltage = msg.power_v();
        buffer_s.CPUtemp = msg.temperature_ntc1();
        buffer_s.fanFrequency = msg.fan_frequency();
        buffer_s.tick = msg.tick();

        // Forward every sample: one take() can return several, and each one is a separate 2 ms tick.
        this->callback(std::move(buffer_s));
    }
}

void SensorDataListenerLowState::on_subscription_matched(dds::sub::DataReader<MsgType>& reader,
                                                 const dds::core::status::SubscriptionMatchedStatus& status)
{
    std::cout << "on_subscription_matched" << std::endl;
}

void SensorDataListenerLowState::on_sample_lost(dds::sub::DataReader<MsgType>& reader, const dds::core::status::SampleLostStatus& status)
{
    std::cout << "on_sample_lost" << std::endl;
}

void SensorDataListenerLowState::on_requested_deadline_missed(dds::sub::DataReader<MsgType>& reader,
                                                      const dds::core::status::RequestedDeadlineMissedStatus& status)
{
    std::cout << "on_requested_deadline_missed" << std::endl;
}

void SensorDataListenerLowState::on_requested_incompatible_qos(dds::sub::DataReader<MsgType>& reader,
                                                       const dds::core::status::RequestedIncompatibleQosStatus& status)
{
    std::cout << "on_requested_incompatible_qos" << std::endl;
}

void SensorDataListenerLowState::on_sample_rejected(dds::sub::DataReader<MsgType>& reader, const dds::core::status::SampleRejectedStatus& status)
{
    std::cout << "on_sample_rejected" << std::endl;
}

void SensorDataListenerLowState::on_liveliness_changed(dds::sub::DataReader<MsgType>& reader,
                                               const dds::core::status::LivelinessChangedStatus& status)
{
    std::cout << "on_liveliness_changed" << std::endl;
}

END_NAMESPACE_UNITREE_MODULE