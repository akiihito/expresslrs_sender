#include "safety_monitor.hpp"

#include <csignal>
#include <cstring>
#include <unistd.h>

#include <spdlog/spdlog.h>

namespace elrs {
namespace safety {

std::atomic<SafetyMonitor*> SafetyMonitor::s_instance{nullptr};
std::atomic<bool> SafetyMonitor::s_shutdown_requested{false};

SafetyMonitor::SafetyMonitor()
    : m_state(SafetyState::Disarmed) {
    m_last_frame_time = std::chrono::steady_clock::now();
}

SafetyMonitor::~SafetyMonitor() {
    // Clear static instance if this was it
    SafetyMonitor* expected = this;
    s_instance.compare_exchange_strong(expected, nullptr);
}

void SafetyMonitor::setConfig(const SafetyConfig& config) {
    m_config = config;
}

void SafetyMonitor::processChannels(ChannelData& channels) {
    SafetyState current_state = m_state.load();

    // Emergency stop takes precedence
    if (current_state == SafetyState::EmergencyStop) {
        channels = getFailsafeChannels();
        return;
    }

    // Check failsafe
    if (current_state == SafetyState::Failsafe) {
        channels = getFailsafeChannels();
        return;
    }

    // Check arm request in channels
    bool arm_requested = isArmRequested(channels);

    switch (current_state) {
        case SafetyState::Disarmed:
            // Force throttle to minimum when disarmed
            channels[2] = m_config.throttle_min;

            if (arm_requested) {
                // Start arm delay
                m_arm_request_time = std::chrono::steady_clock::now();
                m_state = SafetyState::ArmPending;
                spdlog::info("Arm requested, waiting {}ms", m_config.arm_delay_ms);
            }
            break;

        case SafetyState::ArmPending:
            // Force throttle to minimum during arm delay
            channels[2] = m_config.throttle_min;

            if (!arm_requested) {
                // Arm cancelled
                m_state = SafetyState::Disarmed;
                spdlog::info("Arm cancelled");
            } else {
                // Check if delay has passed
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - m_arm_request_time
                );

                if (elapsed.count() >= static_cast<int64_t>(m_config.arm_delay_ms)) {
                    m_state = SafetyState::Armed;
                    spdlog::warn("ARMED - throttle enabled");
                }
            }
            break;

        case SafetyState::Armed:
            if (!arm_requested) {
                // Disarm
                m_state = SafetyState::Disarmed;
                channels[2] = m_config.throttle_min;
                spdlog::info("Disarmed");
            }
            // When armed, pass through throttle as-is
            break;

        default:
            break;
    }
}

bool SafetyMonitor::isArmRequested(const ChannelData& channels) const {
    if (m_config.arm_channel < 0 ||
        m_config.arm_channel >= static_cast<int>(CRSF_MAX_CHANNELS)) {
        return false;
    }

    return channels[m_config.arm_channel] > m_config.arm_threshold;
}

void SafetyMonitor::requestArm() {
    SafetyState expected = SafetyState::Disarmed;
    if (m_state.compare_exchange_strong(expected, SafetyState::ArmPending)) {
        m_arm_request_time = std::chrono::steady_clock::now();
        spdlog::info("Manual arm requested");
    }
}

void SafetyMonitor::requestDisarm() {
    SafetyState current = m_state.load();
    if (current == SafetyState::Armed || current == SafetyState::ArmPending) {
        m_state = SafetyState::Disarmed;
        spdlog::info("Manual disarm");
    }
}

void SafetyMonitor::emergencyStop() {
    SafetyState prev = m_state.exchange(SafetyState::EmergencyStop);
    if (prev != SafetyState::EmergencyStop) {
        spdlog::critical("EMERGENCY STOP");
    }
}

SafetyState SafetyMonitor::getState() const {
    return m_state.load();
}

bool SafetyMonitor::isArmed() const {
    return m_state.load() == SafetyState::Armed;
}

void SafetyMonitor::notifyFrameSent() {
    m_last_frame_time = std::chrono::steady_clock::now();

    // Reset failsafe if we were in it
    SafetyState expected = SafetyState::Failsafe;
    if (m_state.compare_exchange_strong(expected, SafetyState::Disarmed)) {
        spdlog::info("Recovered from failsafe");
    }
}

void SafetyMonitor::checkFailsafe() {
    SafetyState current = m_state.load();

    // Don't override emergency stop
    if (current == SafetyState::EmergencyStop) {
        return;
    }

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_last_frame_time
    );

    if (elapsed.count() >= static_cast<int64_t>(m_config.failsafe_timeout_ms)) {
        SafetyState expected = current;
        if (current != SafetyState::Failsafe &&
            m_state.compare_exchange_strong(expected, SafetyState::Failsafe)) {
            spdlog::error("FAILSAFE - no frames sent for {}ms", elapsed.count());
        }
    }
}

ChannelData SafetyMonitor::getFailsafeChannels() const {
    ChannelData channels;
    channels.fill(CRSF_CHANNEL_MID);

    // Throttle to minimum
    channels[2] = m_config.throttle_min;

    // Arm switch to disarm
    if (m_config.arm_channel >= 0 &&
        m_config.arm_channel < static_cast<int>(CRSF_MAX_CHANNELS)) {
        channels[m_config.arm_channel] = CRSF_CHANNEL_MIN;
    }

    return channels;
}

void SafetyMonitor::installSignalHandlers(SafetyMonitor* monitor) {
    s_instance.store(monitor);
    s_shutdown_requested.store(false);

    struct sigaction sa{};
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
}

bool SafetyMonitor::isShutdownRequested() {
    return s_shutdown_requested.load();
}

void SafetyMonitor::signalHandler(int signum) {
    s_shutdown_requested.store(true);

    SafetyMonitor* monitor = s_instance.load();
    if (monitor) {
        monitor->emergencyStop();
    }

    const char* msg = (signum == SIGINT)
        ? "\nReceived SIGINT, initiating emergency stop...\n"
        : "\nReceived SIGTERM, initiating emergency stop...\n";

    // Use write() for async-signal-safety
    (void)write(STDERR_FILENO, msg, strlen(msg));
}

}  // namespace safety
}  // namespace elrs
