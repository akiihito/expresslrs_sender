#include <gtest/gtest.h>

#include <fstream>
#include <filesystem>

#include "config/config.hpp"

using namespace elrs;
using namespace elrs::config;

class ConfigTest : public ::testing::Test {
protected:
    std::string test_dir;

    void SetUp() override {
        test_dir = std::filesystem::temp_directory_path() / "elrs_config_test";
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir);
    }

    std::string createFile(const std::string& name, const std::string& content) {
        std::string path = test_dir + "/" + name;
        std::ofstream file(path);
        file << content;
        return path;
    }
};

// CFG-001: Valid config loading
TEST_F(ConfigTest, LoadValidConfig) {
    std::string content = R"({
        "device": {
            "port": "/dev/ttyUSB0",
            "baudrate": 115200
        },
        "playback": {
            "default_rate_hz": 100,
            "arm_delay_ms": 5000
        },
        "safety": {
            "arm_channel": 6,
            "throttle_min": 200,
            "failsafe_timeout_ms": 1000
        },
        "logging": {
            "level": "debug",
            "file": "/tmp/test.log"
        }
    })";

    auto path = createFile("valid.json", content);
    auto result = loadConfig(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.device_port, "/dev/ttyUSB0");
    EXPECT_EQ(result.value.baudrate, 115200);
    EXPECT_EQ(result.value.playback.rate_hz, 100.0);
    EXPECT_EQ(result.value.playback.arm_delay_ms, 5000u);
    EXPECT_EQ(result.value.safety.arm_channel, 5);  // 6 - 1 (0-indexed)
    EXPECT_EQ(result.value.safety.throttle_min, 200);
    EXPECT_EQ(result.value.log_level, "debug");
    EXPECT_EQ(result.value.log_file, "/tmp/test.log");
}

// CFG-002: Default values for missing fields
TEST_F(ConfigTest, DefaultValuesForMissingFields) {
    std::string content = R"({
        "device": {
            "port": "/dev/ttyUSB1"
        }
    })";

    auto path = createFile("partial.json", content);
    auto result = loadConfig(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.device_port, "/dev/ttyUSB1");
    EXPECT_EQ(result.value.baudrate, CRSF_BAUDRATE);  // Default (921600)
    EXPECT_EQ(result.value.playback.rate_hz, 500.0);  // Default
    EXPECT_EQ(result.value.safety.arm_channel, 4);     // Default (CH5 - 1)
}

// CFG-003: Invalid JSON syntax
TEST_F(ConfigTest, InvalidJsonSyntax) {
    std::string content = R"({ invalid json )";

    auto path = createFile("invalid.json", content);
    auto result = loadConfig(path);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, ErrorCode::ConfigError);
}

// CFG-004: File not found
TEST_F(ConfigTest, FileNotFound) {
    auto result = loadConfig("/nonexistent/config.json");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, ErrorCode::ConfigError);
}

// CFG-005: Type mismatch (handled gracefully)
TEST_F(ConfigTest, TypeMismatch) {
    std::string content = R"({
        "device": {
            "baudrate": "not_a_number"
        }
    })";

    auto path = createFile("typemismatch.json", content);
    auto result = loadConfig(path);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, ErrorCode::ConfigError);
}

// Default config test
TEST_F(ConfigTest, GetDefaultConfig) {
    auto config = getDefaultConfig();

    EXPECT_EQ(config.device_port, "/dev/ttyAMA0");
    EXPECT_EQ(config.baudrate, CRSF_BAUDRATE);
    EXPECT_EQ(config.playback.rate_hz, 500.0);
    EXPECT_FALSE(config.playback.loop);
    EXPECT_EQ(config.safety.arm_channel, 4);
    EXPECT_EQ(config.safety.throttle_min, CRSF_CHANNEL_MIN);
    EXPECT_EQ(config.log_level, "info");
}

// Empty config file uses defaults
TEST_F(ConfigTest, EmptyConfigUsesDefaults) {
    std::string content = "{}";

    auto path = createFile("empty.json", content);
    auto result = loadConfig(path);

    EXPECT_TRUE(result.ok());
    // Should have default values
    auto defaults = getDefaultConfig();
    EXPECT_EQ(result.value.device_port, defaults.device_port);
    EXPECT_EQ(result.value.baudrate, defaults.baudrate);
}

// CFG-006: gpio_tx resolves device_port
TEST_F(ConfigTest, GpioTxResolvesDevicePort) {
    std::string content = R"({
        "device": {
            "gpio_tx": 4
        }
    })";

    auto path = createFile("gpio.json", content);
    auto result = loadConfig(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.gpio_tx, 4);
    EXPECT_EQ(result.value.device_port, "/dev/ttyAMA2");
}

// CFG-007: gpio_tx with unknown pin keeps default device_port
TEST_F(ConfigTest, GpioTxUnknownPinKeepsDefault) {
    std::string content = R"({
        "device": {
            "gpio_tx": 99
        }
    })";

    auto path = createFile("gpio_unknown.json", content);
    auto result = loadConfig(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.gpio_tx, 99);
    EXPECT_EQ(result.value.device_port, "/dev/ttyAMA0");  // Default unchanged
}

// CFG-008: gpio_tx=-1 (default) does not override device_port
TEST_F(ConfigTest, GpioTxNegativeDoesNotOverride) {
    std::string content = R"({
        "device": {
            "port": "/dev/ttyUSB0"
        }
    })";

    auto path = createFile("gpio_default.json", content);
    auto result = loadConfig(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.gpio_tx, -1);
    EXPECT_EQ(result.value.device_port, "/dev/ttyUSB0");
}

// Nested object missing
TEST_F(ConfigTest, NestedObjectMissing) {
    std::string content = R"({
        "logging": {
            "level": "warn"
        }
    })";

    auto path = createFile("nested.json", content);
    auto result = loadConfig(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.log_level, "warn");
    // device, playback, safety should have defaults
    auto defaults = getDefaultConfig();
    EXPECT_EQ(result.value.device_port, defaults.device_port);
}
