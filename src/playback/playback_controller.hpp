#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <vector>

#include "expresslrs_sender/types.hpp"

namespace elrs {
namespace playback {

// Playback options
struct PlaybackOptions {
    double rate_hz = 500.0;         // Packet send rate
    bool loop = false;              // Loop playback
    int loop_count = 0;             // 0 = infinite
    uint32_t start_time_ms = 0;     // Start position
    uint32_t end_time_ms = 0;       // End position (0 = end of file)
    double speed = 1.0;             // Playback speed multiplier
    uint32_t arm_delay_ms = 3000;   // Delay before arm allowed
};

// Playback statistics
struct PlaybackStats {
    uint64_t frames_sent;
    uint64_t loops_completed;
    uint32_t elapsed_ms;
    double actual_rate_hz;
    double timing_jitter_us;
};

// Callback type for frame sending
using FrameSendCallback = std::function<bool(const ChannelData& channels)>;

class PlaybackController {
public:
    PlaybackController();
    ~PlaybackController();

    // Set frames to play
    void setFrames(std::vector<HistoryFrame> frames);

    // Set options
    void setOptions(const PlaybackOptions& options);

    // Set callback for frame sending
    void setFrameCallback(FrameSendCallback callback);

    // Control playback
    void start();
    void stop();
    void pause();
    void resume();

    // Get state
    PlaybackState getState() const;
    PlaybackStats getStats() const;

    // Get current frame (for dry-run or monitoring)
    const ChannelData& getCurrentFrame() const;

    // Check if playback is complete
    bool isComplete() const;

    // Run one iteration (call in main loop)
    // Returns true if a frame was processed
    bool tick();

private:
    std::vector<HistoryFrame> m_frames;
    PlaybackOptions m_options;
    FrameSendCallback m_callback;

    std::atomic<PlaybackState> m_state;
    std::atomic<bool> m_complete;

    // Timing
    std::chrono::steady_clock::time_point m_start_time;
    std::chrono::steady_clock::time_point m_last_send_time;
    std::chrono::microseconds m_send_interval;

    // Position
    size_t m_current_index;
    uint32_t m_playback_time_ms;
    int m_loops_done;

    // Stats
    uint64_t m_frames_sent;
    double m_jitter_sum;
    uint64_t m_jitter_count;

    // Current frame data
    ChannelData m_current_channels;

    // Find frame index for given timestamp
    size_t findFrameIndex(uint32_t timestamp_ms) const;

    // Update current channels from frame index
    void updateCurrentChannels();

    // Get loop duration
    uint32_t getLoopDuration() const;
};

}  // namespace playback
}  // namespace elrs
