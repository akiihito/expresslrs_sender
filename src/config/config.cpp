#include "config.hpp"

#include <fstream>

#include <nlohmann/json.hpp>

namespace elrs {
namespace config {

using json = nlohmann::json;

AppConfig getDefaultConfig() {
    AppConfig config;

    // Device defaults
    config.device_port = "/dev/ttyAMA0";
    config.baudrate = CRSF_BAUDRATE;

    // Playback defaults
    config.playback.rate_hz = 50.0;
    config.playback.loop = false;
    config.playback.loop_count = 0;
    config.playback.start_time_ms = 0;
    config.playback.end_time_ms = 0;
    config.playback.speed = 1.0;
    config.playback.arm_delay_ms = 3000;

    // Safety defaults
    config.safety.arm_channel = 4;  // CH5
    config.safety.arm_threshold = 1500;
    config.safety.throttle_min = CRSF_CHANNEL_MIN;
    config.safety.failsafe_timeout_ms = 500;
    config.safety.arm_delay_ms = 3000;
    config.safety.disarm_frames = 10;

    // Logging defaults
    config.log_level = "info";

    return config;
}

Result<AppConfig> loadConfig(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return Result<AppConfig>::failure(
            ErrorCode::ConfigError,
            "Cannot open config file: " + filepath
        );
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        return Result<AppConfig>::failure(
            ErrorCode::ConfigError,
            "JSON parse error in config: " + std::string(e.what())
        );
    }

    AppConfig config = getDefaultConfig();

    try {
        // Device settings
        if (j.contains("device")) {
            const auto& device = j["device"];
            if (device.contains("port")) {
                config.device_port = device["port"].get<std::string>();
            }
            if (device.contains("baudrate")) {
                config.baudrate = device["baudrate"].get<int>();
            }
            if (device.contains("invert_tx")) {
                config.invert_tx = device["invert_tx"].get<bool>();
            }
            if (device.contains("invert_rx")) {
                config.invert_rx = device["invert_rx"].get<bool>();
            }
        }

        // Playback settings
        if (j.contains("playback")) {
            const auto& playback = j["playback"];
            if (playback.contains("default_rate_hz")) {
                config.playback.rate_hz = playback["default_rate_hz"].get<double>();
            }
            if (playback.contains("arm_delay_ms")) {
                config.playback.arm_delay_ms = playback["arm_delay_ms"].get<uint32_t>();
            }
        }

        // Safety settings
        if (j.contains("safety")) {
            const auto& safety = j["safety"];
            if (safety.contains("arm_channel")) {
                config.safety.arm_channel = safety["arm_channel"].get<int>() - 1;  // Convert to 0-indexed
            }
            if (safety.contains("arm_threshold")) {
                config.safety.arm_threshold = safety["arm_threshold"].get<int16_t>();
            }
            if (safety.contains("throttle_min")) {
                config.safety.throttle_min = safety["throttle_min"].get<int16_t>();
            }
            if (safety.contains("failsafe_timeout_ms")) {
                config.safety.failsafe_timeout_ms = safety["failsafe_timeout_ms"].get<uint32_t>();
            }
            if (safety.contains("arm_delay_ms")) {
                config.safety.arm_delay_ms = safety["arm_delay_ms"].get<uint32_t>();
            }
            if (safety.contains("disarm_frames")) {
                config.safety.disarm_frames = safety["disarm_frames"].get<int>();
            }
        }

        // Logging settings
        if (j.contains("logging")) {
            const auto& logging = j["logging"];
            if (logging.contains("level")) {
                config.log_level = logging["level"].get<std::string>();
            }
            if (logging.contains("file")) {
                config.log_file = logging["file"].get<std::string>();
            }
        }

    } catch (const json::exception& e) {
        return Result<AppConfig>::failure(
            ErrorCode::ConfigError,
            "Error reading config values: " + std::string(e.what())
        );
    }

    return Result<AppConfig>::success(config);
}

}  // namespace config
}  // namespace elrs
