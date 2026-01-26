#include <gtest/gtest.h>

#include "safety/safety_monitor.hpp"

using namespace elrs;
using namespace elrs::safety;

class SafetyTest : public ::testing::Test {
protected:
    SafetyMonitor monitor;
    SafetyConfig config;

    void SetUp() override {
        config.arm_channel = 4;  // CH5
        config.arm_threshold = 1500;
        config.throttle_min = CRSF_CHANNEL_MIN;
        config.failsafe_timeout_ms = 100;
        config.arm_delay_ms = 50;  // Short delay for testing
        config.disarm_frames = 5;
        monitor.setConfig(config);
    }

    ChannelData createChannels(int16_t arm_value = CRSF_CHANNEL_MIN) {
        ChannelData ch;
        ch.fill(CRSF_CHANNEL_MID);
        ch[2] = 500;  // Some throttle
        ch[4] = arm_value;  // Arm channel
        return ch;
    }
};

// ARM-001: Disarm state forces throttle to min
TEST_F(SafetyTest, DisarmForcesThrottleMin) {
    auto channels = createChannels(CRSF_CHANNEL_MIN);
    channels[2] = 1000;  // Set some throttle

    monitor.processChannels(channels);

    EXPECT_EQ(channels[2], CRSF_CHANNEL_MIN);
    EXPECT_EQ(monitor.getState(), SafetyState::Disarmed);
}

// ARM-002: Armed state allows throttle
TEST_F(SafetyTest, ArmedAllowsThrottle) {
    // First arm
    auto channels = createChannels(CRSF_CHANNEL_MAX);
    monitor.processChannels(channels);

    // Wait for arm delay
    std::this_thread::sleep_for(std::chrono::milliseconds(config.arm_delay_ms + 10));

    channels = createChannels(CRSF_CHANNEL_MAX);
    channels[2] = 1000;
    monitor.processChannels(channels);

    EXPECT_EQ(monitor.getState(), SafetyState::Armed);
    EXPECT_EQ(channels[2], 1000);  // Throttle not modified
}

// ARM-003: Arm transition has delay
TEST_F(SafetyTest, ArmTransitionDelay) {
    auto channels = createChannels(CRSF_CHANNEL_MAX);

    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::ArmPending);

    // Still pending
    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::ArmPending);

    // Wait for delay
    std::this_thread::sleep_for(std::chrono::milliseconds(config.arm_delay_ms + 10));

    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::Armed);
}

// ARM-004: Emergency disarm
TEST_F(SafetyTest, EmergencyDisarm) {
    // Arm first
    auto channels = createChannels(CRSF_CHANNEL_MAX);
    monitor.processChannels(channels);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.arm_delay_ms + 10));
    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::Armed);

    // Emergency stop
    monitor.emergencyStop();
    EXPECT_EQ(monitor.getState(), SafetyState::EmergencyStop);

    // Channels should be set to failsafe
    monitor.processChannels(channels);
    EXPECT_EQ(channels[2], CRSF_CHANNEL_MIN);
}

// FS-001: Failsafe timeout
TEST_F(SafetyTest, FailsafeTimeout) {
    monitor.notifyFrameSent();

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(config.failsafe_timeout_ms + 20));

    monitor.checkFailsafe();
    EXPECT_EQ(monitor.getState(), SafetyState::Failsafe);
}

// FS-002: Failsafe channel values
TEST_F(SafetyTest, FailsafeChannelValues) {
    auto failsafe = monitor.getFailsafeChannels();

    EXPECT_EQ(failsafe[2], CRSF_CHANNEL_MIN);  // Throttle min
    EXPECT_EQ(failsafe[config.arm_channel], CRSF_CHANNEL_MIN);  // Arm off
}

// FS-003: Recovery from failsafe
TEST_F(SafetyTest, FailsafeRecovery) {
    // Trigger failsafe
    std::this_thread::sleep_for(std::chrono::milliseconds(config.failsafe_timeout_ms + 20));
    monitor.checkFailsafe();
    EXPECT_EQ(monitor.getState(), SafetyState::Failsafe);

    // Send a frame
    monitor.notifyFrameSent();
    EXPECT_EQ(monitor.getState(), SafetyState::Disarmed);
}

// Arm request detection
TEST_F(SafetyTest, IsArmRequested) {
    auto armed = createChannels(CRSF_CHANNEL_MAX);
    auto disarmed = createChannels(CRSF_CHANNEL_MIN);

    EXPECT_TRUE(monitor.isArmRequested(armed));
    EXPECT_FALSE(monitor.isArmRequested(disarmed));
}

// Manual arm/disarm
TEST_F(SafetyTest, ManualArmDisarm) {
    monitor.requestArm();
    EXPECT_EQ(monitor.getState(), SafetyState::ArmPending);

    monitor.requestDisarm();
    EXPECT_EQ(monitor.getState(), SafetyState::Disarmed);
}

// isArmed helper
TEST_F(SafetyTest, IsArmedHelper) {
    EXPECT_FALSE(monitor.isArmed());

    // Arm
    auto channels = createChannels(CRSF_CHANNEL_MAX);
    monitor.processChannels(channels);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.arm_delay_ms + 10));
    monitor.processChannels(channels);

    EXPECT_TRUE(monitor.isArmed());
}

// Arm cancelled when switch released
TEST_F(SafetyTest, ArmCancelled) {
    auto channels = createChannels(CRSF_CHANNEL_MAX);
    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::ArmPending);

    // Release arm switch before delay completes
    channels[4] = CRSF_CHANNEL_MIN;
    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::Disarmed);
}

// Disarm from armed state
TEST_F(SafetyTest, DisarmFromArmed) {
    // Arm
    auto channels = createChannels(CRSF_CHANNEL_MAX);
    monitor.processChannels(channels);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.arm_delay_ms + 10));
    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::Armed);

    // Disarm
    channels[4] = CRSF_CHANNEL_MIN;
    monitor.processChannels(channels);
    EXPECT_EQ(monitor.getState(), SafetyState::Disarmed);
}

// Emergency stop overrides everything
TEST_F(SafetyTest, EmergencyStopOverrides) {
    monitor.emergencyStop();

    // Try to arm
    auto channels = createChannels(CRSF_CHANNEL_MAX);
    monitor.processChannels(channels);
    std::this_thread::sleep_for(std::chrono::milliseconds(config.arm_delay_ms + 10));
    monitor.processChannels(channels);

    // Should still be in emergency stop
    EXPECT_EQ(monitor.getState(), SafetyState::EmergencyStop);
}
