#pragma once

#include <atomic>
#include <chrono>
#include <functional>

#include "expresslrs_sender/types.hpp"

namespace elrs {
namespace safety {

// Safety configuration
struct SafetyConfig {
    int arm_channel = 4;              // 0-indexed (CH5)
    int16_t arm_threshold = 1500;     // Value above which is considered armed
    int16_t throttle_min = CRSF_CHANNEL_MIN;
    uint32_t failsafe_timeout_ms = 500;
    uint32_t arm_delay_ms = 3000;     // Delay before arm is allowed
    int disarm_frames = 10;           // Frames to send on emergency stop
};

// Safety state
enum class SafetyState {
    Disarmed,
    ArmPending,   // Waiting for arm delay
    Armed,
    Failsafe,
    EmergencyStop
};

class SafetyMonitor {
public:
    SafetyMonitor();
    ~SafetyMonitor();

    // Configuration
    void setConfig(const SafetyConfig& config);
    const SafetyConfig& getConfig() const { return m_config; }

    // Process channels through safety checks
    // Modifies channels in-place if safety override is needed
    void processChannels(ChannelData& channels);

    // Check if arming is requested in the given channels
    bool isArmRequested(const ChannelData& channels) const;

    // Manual arm/disarm
    void requestArm();
    void requestDisarm();

    // Emergency stop (called on SIGINT/SIGTERM)
    void emergencyStop();

    // Get current state
    SafetyState getState() const;
    bool isArmed() const;

    // Failsafe handling
    void notifyFrameSent();  // Call after each successful frame send
    void checkFailsafe();    // Call periodically to check timeout

    // Get failsafe/disarm channel data
    ChannelData getFailsafeChannels() const;

    // Install signal handlers (SIGINT, SIGTERM)
    static void installSignalHandlers(SafetyMonitor* monitor);

    // Check if shutdown was requested via signal
    static bool isShutdownRequested();

private:
    SafetyConfig m_config;
    std::atomic<SafetyState> m_state;
    std::chrono::steady_clock::time_point m_last_frame_time;
    std::chrono::steady_clock::time_point m_arm_request_time;

    static std::atomic<SafetyMonitor*> s_instance;
    static std::atomic<bool> s_shutdown_requested;

    static void signalHandler(int signum);
};

}  // namespace safety
}  // namespace elrs
