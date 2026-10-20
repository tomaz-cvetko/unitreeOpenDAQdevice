#include <unitree_module/sensor_data_listener.h>
#include <ctime>
#include <chrono>
#include <iostream>

BEGIN_NAMESPACE_UNITREE_MODULE

// SensorDataListenerLowState::SensorDataListenerLowState(const std::function<void(std::vector<int16_t>&&)>& func)
// SensorDataListenerLowState::SensorDataListenerLowState(const std::function<void(BufferS&&)>& func)
//     : callback(func)
SensorDataListenerLowState::SensorDataListenerLowState(std::function<void(BufferS&&)> func)
    : callback(std::move(func))
{
    std::cout << "Hello from SensorDataListenerLowState!" << std::endl;
    printOnce = true;
}

void SensorDataListenerLowState::on_data_available(dds::sub::DataReader<MsgType>& reader)
{
    time_t timestamp;
    time(&timestamp);
    
    dds::sub::LoanedSamples<MsgType> samples = reader.take();
    size_t numberOfSamples = samples.length();
    
    constexpr size_t numberOfComponents = 4;
    std::vector<int16_t> buffer(numberOfComponents * numberOfSamples, 0);

    // std::vector<float_t> buffer(numberOfComponents * numberOfSamples, 0);
    if (printOnce)
    {
        std::cout << "buffer: ";
        for (size_t i = 0; i < buffer.size(); ++i)
        {
            std::cout << buffer[i] << " ";
        }
        std::cout << std::endl;
    }
    
    size_t sampleIndex = 0;
    for (const auto& sample : samples)
    {
        if (!sample.info().valid())
        {
            continue;
        }
        
        const MsgType& msg = sample.data();
        const std::array<int16_t, 4>& values = msg.foot_force();
        // const std::array<float_t, 3>& values2 = msg.imu_state().accelerometer();

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

        /*std::cout << "quaternion: ";
        for (int i = 0; i < 4; i++)
        {
            std::cout << msg.imu_state().quaternion()[i] << ", ";
        }
        std::cout << "\n";*/
        /*std::cout << "motor ddq: ";
        for (int i = 0; i < 12; i++)
        {
            std::cout << msg.motor_state()[i].ddq() << ", ";
        }
        std::cout << "\n";*/
        /*std::cout << "motor temp: ";
        for (int i = 0; i < 12; i++)
        {
            std::cout << int(msg.motor_state()[i].temperature()) << ", ";
        }
        std::cout << "\n";*/

        // std::cout << "Battery: " << +buffer_s.battery << "%\n";

        // Pack into flat array as [x0, y0, z0, w0, x1, y1, z1, w1]
        //std::copy(values.data(), values.data() + 4, &buffer[4 * sampleIndex]);

        // Equivalent but a bit easer to control
        for (size_t i = 0; i < 4; ++i)
        {
            const size_t index = numberOfComponents * sampleIndex + i;
            buffer[index] = values[i];
            // std::cout << values[i] << "\n";
            // buffer_s.forces[index] = values[i];
            if (printOnce)
            {
                std::cout << "forces: i = " << i << ", index = " << index << ", buffer = " << float(buffer[index]) << std::endl;
            }
        }
        /* for (size_t i = 4; i < 7; ++i)
        {
            const size_t index = numberOfComponents * sampleIndex + i;
            buffer[index] = values2[i-4];
            if (printOnce)
            {
                std::cout << "IMU: i = " << i << ", index = " << index << ", buffer = " << buffer[index] << std::endl;
            }
        } */
        // buffer[4] = msg.bms_state().soc();
        if (printOnce)
        {
            std::cout << "buffer: ";
            for (size_t i = 0; i < buffer.size(); ++i)
            {
                std::cout << buffer[i] << " ";
            }
            std::cout << std::endl;
            printOnce = false;
        }
        // buffer[numberOfComponents * sampleIndex + 7] = msg.bms_state().soc();
        // std::cout << int(msg.bms_state().soc()) << "%\n";

        // std::cout << "force = " << buffer[0] << "N, battery = " << buffer[7] << "%\n";

        // Question: Is there any information about the time these values were measured that we must propagate?

        // Forward every sample: one take() can return several, and each one is a separate 2 ms tick.
        // (Invalid samples, e.g. dispose/unregister notifications, carry no data and are skipped above.)
        this->callback(std::move(buffer_s));

        ++sampleIndex;
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