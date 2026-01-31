#pragma once

#include <string>

#include "expresslrs_sender/types.hpp"
#include "playback/playback_controller.hpp"
#include "safety/safety_monitor.hpp"

namespace elrs {
namespace config {

// Application configuration
struct AppConfig {
    // Device settings
    std::string device_port = "/dev/ttyAMA0";
    int baudrate = CRSF_BAUDRATE;
    bool invert_tx = true;   // UART TX信号反転（ELRS TX モジュールでは必要）
    bool invert_rx = false;  // UART RX信号反転
    bool half_duplex = true; // 半二重通信（S.Port 1本接続）
    int gpio_tx = -1;        // GPIO TXピン番号（-1 = 未指定）

    // Playback defaults
    playback::PlaybackOptions playback;

    // Safety settings
    safety::SafetyConfig safety;

    // Logging
    std::string log_level = "info";
    std::string log_file;
};

// Load configuration from JSON file
Result<AppConfig> loadConfig(const std::string& filepath);

// Get default configuration
AppConfig getDefaultConfig();

}  // namespace config
}  // namespace elrs
