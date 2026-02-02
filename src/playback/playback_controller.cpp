#include "playback_controller.hpp"

#include <algorithm>
#include <cmath>
#include <thread>

namespace elrs {
namespace playback {

PlaybackController::PlaybackController()
    : m_state(PlaybackState::Stopped)
    , m_complete(false)
    , m_current_index(0)
    , m_playback_time_ms(0)
    , m_loops_done(0)
    , m_frames_sent(0)
    , m_jitter_sum(0)
    , m_jitter_count(0)
    , m_max_jitter(0) {
    // Initialize channels to center
    m_current_channels.fill(CRSF_CHANNEL_MID);
}

PlaybackController::~PlaybackController() {
    stop();
}

void PlaybackController::setFrames(std::vector<HistoryFrame> frames) {
    m_frames = std::move(frames);
}

void PlaybackController::setOptions(const PlaybackOptions& options) {
    m_options = options;

    // Calculate send interval from rate
    double interval_us = 1000000.0 / options.rate_hz;
    m_send_interval = std::chrono::microseconds(static_cast<int64_t>(interval_us));
}

void PlaybackController::setFrameCallback(FrameSendCallback callback) {
    m_callback = std::move(callback);
}

void PlaybackController::start() {
    if (m_frames.empty()) {
        return;
    }

    m_state = PlaybackState::Playing;
    m_complete = false;
    m_current_index = 0;
    m_playback_time_ms = m_options.start_time_ms;
    m_loops_done = 0;
    m_frames_sent = 0;
    m_jitter_sum = 0;
    m_jitter_count = 0;
    m_max_jitter = 0;

    // Find starting frame
    m_current_index = findFrameIndex(m_options.start_time_ms);

    m_start_time = std::chrono::steady_clock::now();
    m_last_send_time = m_start_time;

    updateCurrentChannels();
}

void PlaybackController::stop() {
    m_state = PlaybackState::Stopped;
    m_current_channels.fill(CRSF_CHANNEL_MID);
    // Set throttle to minimum for safety
    m_current_channels[2] = CRSF_CHANNEL_MIN;
}

void PlaybackController::pause() {
    if (m_state == PlaybackState::Playing) {
        m_state = PlaybackState::Paused;
    }
}

void PlaybackController::resume() {
    if (m_state == PlaybackState::Paused) {
        m_state = PlaybackState::Playing;
        m_last_send_time = std::chrono::steady_clock::now();
    }
}

PlaybackState PlaybackController::getState() const {
    return m_state.load();
}

PlaybackStats PlaybackController::getStats() const {
    PlaybackStats stats{};
    stats.frames_sent = m_frames_sent;
    stats.loops_completed = static_cast<uint64_t>(m_loops_done);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    stats.elapsed_ms = static_cast<uint32_t>(elapsed.count());

    if (stats.elapsed_ms > 0) {
        stats.actual_rate_hz = static_cast<double>(m_frames_sent) * 1000.0 /
            static_cast<double>(stats.elapsed_ms);
    }

    if (m_jitter_count > 0) {
        stats.timing_jitter_us = m_jitter_sum / static_cast<double>(m_jitter_count);
    }

    stats.max_jitter_us = m_max_jitter;

    return stats;
}

const ChannelData& PlaybackController::getCurrentFrame() const {
    return m_current_channels;
}

bool PlaybackController::isComplete() const {
    return m_complete.load();
}

size_t PlaybackController::findFrameIndex(uint32_t timestamp_ms) const {
    if (m_frames.empty()) {
        return 0;
    }

    // Binary search for the frame at or before timestamp
    auto it = std::lower_bound(
        m_frames.begin(), m_frames.end(), timestamp_ms,
        [](const HistoryFrame& frame, uint32_t ts) {
            return frame.timestamp_ms < ts;
        }
    );

    if (it == m_frames.end()) {
        return m_frames.size() - 1;
    }

    if (it == m_frames.begin()) {
        return 0;
    }

    // Return the frame at or just before the timestamp
    if (it->timestamp_ms > timestamp_ms) {
        --it;
    }

    return static_cast<size_t>(std::distance(m_frames.begin(), it));
}

void PlaybackController::updateCurrentChannels() {
    if (m_current_index < m_frames.size()) {
        m_current_channels = m_frames[m_current_index].channels;
    }
}

bool PlaybackController::tick() {
    if (m_state != PlaybackState::Playing || m_frames.empty()) {
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    auto since_last = std::chrono::duration_cast<std::chrono::microseconds>(now - m_last_send_time);

    // Check if it's time to send
    if (since_last < m_send_interval) {
        return false;
    }

    // Calculate timing jitter
    int64_t jitter = since_last.count() - m_send_interval.count();
    double abs_jitter = std::abs(static_cast<double>(jitter));
    m_jitter_sum += abs_jitter;
    m_jitter_count++;
    if (abs_jitter > m_max_jitter) {
        m_max_jitter = abs_jitter;
    }

    // Drift correction: advance by exact interval instead of snapping to now
    m_last_send_time += m_send_interval;
    // Snap forward if more than 3 intervals behind (prevent burst sends)
    if (now - m_last_send_time > m_send_interval * 3) {
        m_last_send_time = now;
    }

    // Update playback time
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start_time);
    m_playback_time_ms = m_options.start_time_ms +
        static_cast<uint32_t>(elapsed.count() * m_options.speed);

    // Account for loops
    if (m_loops_done > 0) {
        uint32_t loop_duration = getLoopDuration();
        m_playback_time_ms = m_options.start_time_ms +
            ((m_playback_time_ms - m_options.start_time_ms) % loop_duration);
    }

    // Check end condition
    uint32_t end_time = m_options.end_time_ms;
    if (end_time == 0 && !m_frames.empty()) {
        end_time = m_frames.back().timestamp_ms;
    }

    if (m_playback_time_ms >= end_time) {
        // End of playback
        if (m_options.loop) {
            m_loops_done++;

            // Check loop count limit
            if (m_options.loop_count > 0 && m_loops_done >= m_options.loop_count) {
                m_complete = true;
                m_state = PlaybackState::Stopped;
                return false;
            }

            // Reset for next loop
            m_playback_time_ms = m_options.start_time_ms;
            m_current_index = findFrameIndex(m_options.start_time_ms);
            m_start_time = now;
        } else {
            m_complete = true;
            m_state = PlaybackState::Stopped;
            return false;
        }
    }

    // Find and update current frame
    m_current_index = findFrameIndex(m_playback_time_ms);
    updateCurrentChannels();

    // Send frame via callback
    if (m_callback) {
        if (!m_callback(m_current_channels)) {
            // Callback returned false, stop playback
            m_state = PlaybackState::Stopped;
            return false;
        }
    }

    m_frames_sent++;
    return true;
}

uint32_t PlaybackController::getLoopDuration() const {
    uint32_t end_time = m_options.end_time_ms;
    if (end_time == 0 && !m_frames.empty()) {
        end_time = m_frames.back().timestamp_ms;
    }
    return end_time - m_options.start_time_ms;
}

}  // namespace playback
}  // namespace elrs
