/**
 * dog_simulator: publishes fake Unitree Go2 LowState messages on the DDS topic "rt/lowstate",
 * so the module can be tested without the robot. Run it next to server_example (same PC or same network).
 *
 * Like the real dog it publishes at 500 Hz and increments LowState::tick by 2 (ms) per message.
 * It can also inject the network problems that matter for timestamping:
 *   --drop <percent>     randomly skip messages (gaps: tick jumps by more than 2)
 *   --stall <ms>         every --stall-every seconds, hold messages back for <ms> and then send the
 *                        backlog at once (bursts: several messages arrive together, ticks unchanged)
 *
 * Signal values are deterministic so they are easy to check in a client:
 *   foot_force[i]          20 + 15*sin(2*pi*0.5*t + i*pi/2)   [N, int16]
 *   motor_state[i].q        0.1*i + 0.5*sin(2*pi*t)            [rad]
 *   motor_state[i].temperature  30 + i                         [degC, uint8: 30..41]
 *   power_a                1.5 + 0.5*sin(2*pi*0.2*t)          [A, float, non-integer]
 *   power_v                28.5 [V], bms soc 80 [%], temperature_ntc1 45 [degC]
 */

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <thread>

#include "dds/dds.hpp"
#include "LowState.hpp"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <timeapi.h>
#endif

namespace
{
    using LowState = unitree_go::msg::dds_::LowState_;
    constexpr double Pi = 3.14159265358979323846;

    std::atomic<bool> stopRequested{false};

    struct Options
    {
        double rateHz = 500.0;
        double durationS = 0.0;  // 0 = until Ctrl+C
        double dropPercent = 0.0;
        double stallMs = 0.0;
        double stallEveryS = 10.0;
        int domain = 0;
        uint32_t tickStart = 1000;
    };

    void printUsage()
    {
        std::cout << "Usage: dog_simulator [options]\n"
                     "  --rate <Hz>          publish rate (default 500, like the Go2)\n"
                     "  --duration <s>       stop after this many seconds (default: run until Ctrl+C)\n"
                     "  --drop <percent>     randomly drop this percentage of messages (default 0)\n"
                     "  --stall <ms>         periodically hold messages back for <ms>, then send them as a burst (default 0 = off)\n"
                     "  --stall-every <s>    period of the stalls (default 10)\n"
                     "  --tick-start <ms>    first LowState::tick value (default 1000)\n"
                     "  --domain <id>        DDS domain (default 0)\n";
    }

    bool parseOptions(int argc, char* argv[], Options& opt)
    {
        for (int i = 1; i < argc; ++i)
        {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h")
                return false;
            if (i + 1 >= argc)
            {
                std::cerr << "Missing value for " << arg << "\n";
                return false;
            }
            const char* value = argv[++i];
            if (arg == "--rate")
                opt.rateHz = std::atof(value);
            else if (arg == "--duration")
                opt.durationS = std::atof(value);
            else if (arg == "--drop")
                opt.dropPercent = std::atof(value);
            else if (arg == "--stall")
                opt.stallMs = std::atof(value);
            else if (arg == "--stall-every")
                opt.stallEveryS = std::atof(value);
            else if (arg == "--tick-start")
                opt.tickStart = static_cast<uint32_t>(std::strtoul(value, nullptr, 10));
            else if (arg == "--domain")
                opt.domain = std::atoi(value);
            else
            {
                std::cerr << "Unknown option " << arg << "\n";
                return false;
            }
        }
        return opt.rateHz > 0 && opt.stallEveryS > 0;
    }

    void fillMessage(LowState& msg, uint32_t tick, double t)
    {
        msg.tick(tick);

        for (size_t i = 0; i < 4; ++i)
            msg.foot_force()[i] = static_cast<int16_t>(std::lround(20.0 + 15.0 * std::sin(2 * Pi * 0.5 * t + i * Pi / 2)));

        for (size_t i = 0; i < 12; ++i)
        {
            auto& motor = msg.motor_state()[i];
            motor.q(static_cast<float>(0.1 * i + 0.5 * std::sin(2 * Pi * t)));
            motor.dq(static_cast<float>(Pi * std::cos(2 * Pi * t)));
            motor.tau_est(static_cast<float>(0.01 * i));
            motor.temperature(static_cast<uint8_t>(30 + i));
        }

        auto& imu = msg.imu_state();
        imu.rpy({static_cast<float>(0.05 * std::sin(2 * Pi * 0.3 * t)), 0.02f, 0.0f});
        imu.gyroscope({0.0f, 0.0f, static_cast<float>(0.1 * std::cos(2 * Pi * 0.3 * t))});
        imu.accelerometer({0.0f, 0.0f, 9.81f});
        imu.quaternion({1.0f, 0.0f, 0.0f, 0.0f});
        imu.temperature(35);

        msg.bms_state().soc(80);
        msg.power_a(static_cast<float>(1.5 + 0.5 * std::sin(2 * Pi * 0.2 * t)));
        msg.power_v(28.5f);
        msg.temperature_ntc1(45);
        msg.fan_frequency({1000, 1100, 1200, 0});
    }
}

int main(int argc, char* argv[])
{
    Options opt;
    if (!parseOptions(argc, argv, opt))
    {
        printUsage();
        return 1;
    }

    std::signal(SIGINT, [](int) { stopRequested = true; });

#ifdef _WIN32
    // Default Windows timer granularity is ~15.6 ms; ask for 1 ms so messages are spread evenly.
    timeBeginPeriod(1);
#endif

    try
    {
        dds::domain::DomainParticipant participant(opt.domain);
        dds::topic::Topic<LowState> topic(participant, "rt/lowstate");
        dds::pub::Publisher publisher(participant);
        dds::pub::DataWriter<LowState> writer(publisher, topic);

        const uint32_t tickStepMs = static_cast<uint32_t>(std::lround(1000.0 / opt.rateHz));
        std::cout << "Publishing LowState on 'rt/lowstate' (domain " << opt.domain << ") at " << opt.rateHz << " Hz, tick step "
                  << tickStepMs << " ms";
        if (opt.dropPercent > 0)
            std::cout << ", dropping " << opt.dropPercent << "%";
        if (opt.stallMs > 0)
            std::cout << ", " << opt.stallMs << " ms stall every " << opt.stallEveryS << " s";
        std::cout << ". Ctrl+C to stop." << std::endl;

        std::mt19937 rng(12345);
        std::uniform_real_distribution<double> uniform(0.0, 100.0);

        LowState msg;
        const auto period = std::chrono::duration<double>(1.0 / opt.rateHz);
        const auto start = std::chrono::steady_clock::now();
        auto nextReport = start + std::chrono::seconds(1);

        uint64_t index = 0;  // index of the next message; it determines both tick and simulated time
        uint64_t published = 0, dropped = 0, stalls = 0;
        bool inStall = false;

        while (!stopRequested)
        {
            const auto now = std::chrono::steady_clock::now();
            const double elapsedS = std::chrono::duration<double>(now - start).count();
            if (opt.durationS > 0 && elapsedS >= opt.durationS)
                break;

            // During a stall nothing is sent; the messages pile up and go out as a burst afterwards.
            const bool stallNow = opt.stallMs > 0 && elapsedS > opt.stallEveryS &&
                                  std::fmod(elapsedS, opt.stallEveryS) < opt.stallMs / 1000.0;
            if (stallNow && !inStall)
                ++stalls;
            inStall = stallNow;

            if (!stallNow)
            {
                // Catch up to the schedule: normally one message, after a stall the whole backlog.
                const auto due = static_cast<uint64_t>(elapsedS / period.count()) + 1;
                for (; index < due; ++index)
                {
                    if (opt.dropPercent > 0 && uniform(rng) < opt.dropPercent)
                    {
                        ++dropped;
                        continue;
                    }
                    fillMessage(msg, opt.tickStart + static_cast<uint32_t>(index) * tickStepMs, index * period.count());
                    writer.write(msg);
                    ++published;
                }
            }

            if (now >= nextReport)
            {
                std::cout << "t=" << static_cast<int>(elapsedS) << "s published=" << published << " dropped=" << dropped
                          << " stalls=" << stalls << " tick=" << (opt.tickStart + static_cast<uint32_t>(index) * tickStepMs)
                          << " matched_readers=" << writer.publication_matched_status().current_count() << std::endl;
                nextReport += std::chrono::seconds(1);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::cout << "Done: published=" << published << " dropped=" << dropped << " stalls=" << stalls << std::endl;
    }
    catch (const dds::core::Exception& e)
    {
        std::cerr << "DDS error: " << e.what() << std::endl;
        return 2;
    }

#ifdef _WIN32
    timeEndPeriod(1);
#endif
    return 0;
}
