#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "playback/playback_controller.hpp"

using namespace elrs;
using namespace elrs::playback;

class PlaybackTest : public ::testing::Test {
protected:
    std::vector<HistoryFrame> createFrames(size_t count, uint32_t interval_ms = 20) {
        std::vector<HistoryFrame> frames;
        for (size_t i = 0; i < count; i++) {
            HistoryFrame frame;
            frame.timestamp_ms = static_cast<uint32_t>(i * interval_ms);
            frame.channels.fill(CRSF_CHANNEL_MID);
            frame.channels[2] = static_cast<int16_t>(CRSF_CHANNEL_MIN + i);  // Varying throttle
            frames.push_back(frame);
        }
        return frames;
    }
};

// PLY-001: Start state
TEST_F(PlaybackTest, StartState) {
    PlaybackController controller;
    controller.setFrames(createFrames(10));

    PlaybackOptions options;
    options.rate_hz = 50;
    controller.setOptions(options);

    EXPECT_EQ(controller.getState(), PlaybackState::Stopped);

    controller.start();
    EXPECT_EQ(controller.getState(), PlaybackState::Playing);
}

// PLY-002: Stop state
TEST_F(PlaybackTest, StopState) {
    PlaybackController controller;
    controller.setFrames(createFrames(10));

    PlaybackOptions options;
    options.rate_hz = 50;
    controller.setOptions(options);

    controller.start();
    controller.stop();

    EXPECT_EQ(controller.getState(), PlaybackState::Stopped);
}

// PLY-003: Pause state
TEST_F(PlaybackTest, PauseState) {
    PlaybackController controller;
    controller.setFrames(createFrames(10));

    PlaybackOptions options;
    options.rate_hz = 50;
    controller.setOptions(options);

    controller.start();
    controller.pause();

    EXPECT_EQ(controller.getState(), PlaybackState::Paused);
}

// PLY-004: Resume state
TEST_F(PlaybackTest, ResumeState) {
    PlaybackController controller;
    controller.setFrames(createFrames(10));

    PlaybackOptions options;
    options.rate_hz = 50;
    controller.setOptions(options);

    controller.start();
    controller.pause();
    controller.resume();

    EXPECT_EQ(controller.getState(), PlaybackState::Playing);
}

// PLY-005: Loop playback
TEST_F(PlaybackTest, LoopPlayback) {
    PlaybackController controller;
    controller.setFrames(createFrames(5, 10));  // 50ms total

    PlaybackOptions options;
    options.rate_hz = 100;
    options.loop = true;
    options.loop_count = 2;
    controller.setOptions(options);

    int frames_sent = 0;
    controller.setFrameCallback([&](const ChannelData&) {
        frames_sent++;
        return true;
    });

    controller.start();

    // Run for enough time to complete 2 loops
    auto start = std::chrono::steady_clock::now();
    while (!controller.isComplete()) {
        controller.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        // Safety timeout
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > 500) break;
    }

    EXPECT_TRUE(controller.isComplete());
    auto stats = controller.getStats();
    EXPECT_EQ(stats.loops_completed, 2u);
}

// PLY-006: Loop count limit
TEST_F(PlaybackTest, LoopCountLimit) {
    PlaybackController controller;
    controller.setFrames(createFrames(3, 10));

    PlaybackOptions options;
    options.rate_hz = 100;
    options.loop = true;
    options.loop_count = 3;
    controller.setOptions(options);

    controller.start();

    auto start = std::chrono::steady_clock::now();
    while (!controller.isComplete()) {
        controller.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > 500) break;
    }

    auto stats = controller.getStats();
    EXPECT_LE(stats.loops_completed, 3u);
}

// GET-001: Get current frame
TEST_F(PlaybackTest, GetCurrentFrame) {
    PlaybackController controller;
    auto frames = createFrames(10);
    controller.setFrames(frames);

    PlaybackOptions options;
    options.rate_hz = 50;
    controller.setOptions(options);

    controller.start();

    const auto& current = controller.getCurrentFrame();
    // Should have valid channel data
    EXPECT_GE(current[0], CRSF_CHANNEL_MIN);
    EXPECT_LE(current[0], CRSF_CHANNEL_MAX);
}

// Callback test
TEST_F(PlaybackTest, FrameCallback) {
    PlaybackController controller;
    controller.setFrames(createFrames(5, 20));

    PlaybackOptions options;
    options.rate_hz = 50;
    controller.setOptions(options);

    int callback_count = 0;
    controller.setFrameCallback([&](const ChannelData& ch) {
        callback_count++;
        EXPECT_EQ(ch[0], CRSF_CHANNEL_MID);
        return true;
    });

    controller.start();

    // Run for a short time
    auto start = std::chrono::steady_clock::now();
    while (!controller.isComplete()) {
        controller.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > 200) break;
    }

    EXPECT_GT(callback_count, 0);
}

// Callback return false stops playback
TEST_F(PlaybackTest, CallbackStopsPlayback) {
    PlaybackController controller;
    controller.setFrames(createFrames(100));

    PlaybackOptions options;
    options.rate_hz = 1000;
    controller.setOptions(options);

    int callback_count = 0;
    controller.setFrameCallback([&](const ChannelData&) {
        callback_count++;
        return callback_count < 5;  // Stop after 5 frames
    });

    controller.start();

    while (controller.getState() == PlaybackState::Playing) {
        controller.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    EXPECT_EQ(callback_count, 5);
    EXPECT_EQ(controller.getState(), PlaybackState::Stopped);
}

// Stats test
TEST_F(PlaybackTest, Stats) {
    PlaybackController controller;
    controller.setFrames(createFrames(10, 20));

    PlaybackOptions options;
    options.rate_hz = 50;
    controller.setOptions(options);

    controller.setFrameCallback([](const ChannelData&) { return true; });
    controller.start();

    // Run for a bit
    auto start = std::chrono::steady_clock::now();
    while (!controller.isComplete()) {
        controller.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > 300) break;
    }

    auto stats = controller.getStats();
    EXPECT_GT(stats.frames_sent, 0u);
    EXPECT_GT(stats.elapsed_ms, 0u);
}

// Empty frames
TEST_F(PlaybackTest, EmptyFrames) {
    PlaybackController controller;
    controller.setFrames({});

    PlaybackOptions options;
    controller.setOptions(options);

    controller.start();

    // Should not crash, just not do anything
    EXPECT_NE(controller.getState(), PlaybackState::Playing);
}

// Speed multiplier test
TEST_F(PlaybackTest, SpeedMultiplier) {
    PlaybackController controller;
    controller.setFrames(createFrames(10, 100));  // 1000ms total at normal speed

    PlaybackOptions options;
    options.rate_hz = 100;
    options.speed = 2.0;  // 2x speed = 500ms
    controller.setOptions(options);

    controller.setFrameCallback([](const ChannelData&) { return true; });
    controller.start();

    auto start = std::chrono::steady_clock::now();
    while (!controller.isComplete()) {
        controller.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > 1000) break;  // Safety timeout
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    // Should complete in roughly half the time (with some tolerance)
    EXPECT_TRUE(controller.isComplete());
    EXPECT_LT(elapsed.count(), 800);  // Should be around 500ms + overhead
}
