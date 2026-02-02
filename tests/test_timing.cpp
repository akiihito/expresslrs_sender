#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "playback/playback_controller.hpp"

using namespace elrs;
using namespace elrs::playback;

class TimingTest : public ::testing::Test {
protected:
    std::vector<HistoryFrame> createFrames(size_t count, uint32_t interval_ms = 10) {
        std::vector<HistoryFrame> frames;
        for (size_t i = 0; i < count; i++) {
            HistoryFrame frame;
            frame.timestamp_ms = static_cast<uint32_t>(i * interval_ms);
            frame.channels.fill(CRSF_CHANNEL_MID);
            frame.channels[2] = CRSF_CHANNEL_MIN;
            frames.push_back(frame);
        }
        return frames;
    }
};

// TIM-001: Drift correction produces accurate frame count at 100Hz over 500ms
TEST_F(TimingTest, DriftCorrectionAccuracy) {
    PlaybackController controller;
    // Create 1 second of frames at 10ms intervals (100 frames)
    controller.setFrames(createFrames(100, 10));

    PlaybackOptions options;
    options.rate_hz = 100.0;  // 100Hz = 10ms interval
    controller.setOptions(options);

    uint64_t frames_sent = 0;
    controller.setFrameCallback([&](const ChannelData&) {
        frames_sent++;
        return true;
    });

    controller.start();

    // Run for 500ms
    auto start = std::chrono::steady_clock::now();
    auto deadline = start + std::chrono::milliseconds(500);

    while (std::chrono::steady_clock::now() < deadline && !controller.isComplete()) {
        controller.tick();

        // Use the same sleep strategy as production code
        auto now = std::chrono::steady_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::microseconds>(
            deadline - now);
        if (remaining.count() > 500) {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    // At 100Hz for 500ms, we expect ~50 frames. Allow ±3 tolerance.
    EXPECT_GE(frames_sent, 47u) << "Too few frames sent (expected ~50)";
    EXPECT_LE(frames_sent, 53u) << "Too many frames sent (expected ~50)";
}

// TIM-002: Max jitter is tracked
TEST_F(TimingTest, MaxJitterTracked) {
    PlaybackController controller;
    controller.setFrames(createFrames(50, 10));

    PlaybackOptions options;
    options.rate_hz = 100.0;
    controller.setOptions(options);

    controller.setFrameCallback([](const ChannelData&) { return true; });
    controller.start();

    auto start = std::chrono::steady_clock::now();
    while (!controller.isComplete()) {
        controller.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(100));

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > 1000) break;
    }

    auto stats = controller.getStats();
    // max_jitter should be >= avg jitter (or both zero if no frames)
    if (stats.frames_sent > 0) {
        EXPECT_GE(stats.max_jitter_us, stats.timing_jitter_us);
    }
}
