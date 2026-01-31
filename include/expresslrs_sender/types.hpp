#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace elrs {

// CRSF device addresses
constexpr uint8_t CRSF_ADDRESS_BROADCAST = 0x00;              // ブロードキャスト
constexpr uint8_t CRSF_ADDRESS_FLIGHT_CONTROLLER = 0xC8;      // FC宛て
constexpr uint8_t CRSF_ADDRESS_HANDSET = 0xEA;                // ハンドセット（送信機）
constexpr uint8_t CRSF_ADDRESS_TRANSMITTER = 0xEE;            // TXモジュール宛て（Raspberry Pi→TX）
constexpr uint8_t CRSF_SYNC_BYTE = CRSF_ADDRESS_TRANSMITTER;  // デフォルトはTXモジュール宛て

// CRSF frame types
constexpr uint8_t CRSF_FRAME_TYPE_RC_CHANNELS = 0x16;
constexpr uint8_t CRSF_FRAME_TYPE_LINK_STATISTICS = 0x14;
constexpr uint8_t CRSF_FRAME_TYPE_DEVICE_PING = 0x28;
constexpr uint8_t CRSF_FRAME_TYPE_DEVICE_INFO = 0x29;

constexpr size_t CRSF_MAX_FRAME_SIZE = 64;
constexpr size_t CRSF_MAX_CHANNELS = 16;
constexpr size_t CRSF_CHANNEL_BITS = 11;
constexpr size_t CRSF_RC_FRAME_PAYLOAD_SIZE = 22;
constexpr size_t CRSF_RC_FRAME_SIZE = 26;  // Sync + Len + Type + Payload + CRC

// Channel value constants
constexpr int16_t CRSF_CHANNEL_MIN = 172;
constexpr int16_t CRSF_CHANNEL_MID = 992;
constexpr int16_t CRSF_CHANNEL_MAX = 1811;

constexpr int16_t PWM_MIN = 988;
constexpr int16_t PWM_MID = 1500;
constexpr int16_t PWM_MAX = 2012;

// Default UART settings
constexpr int CRSF_BAUDRATE = 921600;      // TX モジュール用 (ELRS V3.x)
constexpr int CRSF_BAUDRATE_RX = 420000;   // レシーバー用

// Channel data type
using ChannelData = std::array<int16_t, CRSF_MAX_CHANNELS>;

// Device information from DEVICE_INFO response
struct DeviceInfo {
    std::string device_name;
    std::array<uint8_t, 4> serial_number{};
    std::array<uint8_t, 4> hardware_id{};
    std::array<uint8_t, 4> firmware_id{};
    uint8_t parameter_count = 0;
    uint8_t parameter_protocol_version = 0;
};

// Single frame in history
struct HistoryFrame {
    uint32_t timestamp_ms;
    ChannelData channels;
};

// Playback state
enum class PlaybackState {
    Stopped,
    Playing,
    Paused
};

// Error codes (matching CLI exit codes)
enum class ErrorCode : int {
    Success = 0,
    GeneralError = 1,
    ArgumentError = 2,
    ConfigError = 3,
    HistoryError = 4,
    DeviceError = 5,
    SafetyError = 6
};

// Result type for operations that can fail
template <typename T>
struct Result {
    T value;
    ErrorCode error;
    std::string message;

    bool ok() const { return error == ErrorCode::Success; }

    static Result success(T val) {
        return Result{std::move(val), ErrorCode::Success, ""};
    }

    static Result failure(ErrorCode err, const std::string& msg) {
        return Result{T{}, err, msg};
    }
};

// Specialization for void
template <>
struct Result<void> {
    ErrorCode error;
    std::string message;

    bool ok() const { return error == ErrorCode::Success; }

    static Result success() {
        return Result{ErrorCode::Success, ""};
    }

    static Result failure(ErrorCode err, const std::string& msg) {
        return Result{err, msg};
    }
};

}  // namespace elrs
