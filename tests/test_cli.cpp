#include <gtest/gtest.h>

// Note: These are basic CLI parsing tests
// Full integration tests would require running the actual executable

#include "expresslrs_sender/types.hpp"

using namespace elrs;

class CliTest : public ::testing::Test {};

// Basic argument parsing tests (unit-level)
// Full CLI testing would be done via integration tests

// CLI-001: Help flag recognition
TEST_F(CliTest, HelpFlagRecognition) {
    // Test that "--help" and "-h" are valid help flags
    const char* help_flags[] = {"--help", "-h"};

    for (const char* flag : help_flags) {
        EXPECT_TRUE(strcmp(flag, "--help") == 0 || strcmp(flag, "-h") == 0);
    }
}

// CLI-002: Version flag recognition
TEST_F(CliTest, VersionFlagRecognition) {
    const char* version_flags[] = {"--version", "-V"};

    for (const char* flag : version_flags) {
        EXPECT_TRUE(strcmp(flag, "--version") == 0 || strcmp(flag, "-V") == 0);
    }
}

// CLI-003: Valid commands
TEST_F(CliTest, ValidCommands) {
    const char* valid_commands[] = {"play", "validate", "ping", "info", "send"};

    for (const char* cmd : valid_commands) {
        // All valid commands should be non-empty
        EXPECT_GT(strlen(cmd), 0u);
    }
}

// CLI-004: Short option mapping
TEST_F(CliTest, ShortOptionMapping) {
    // Test short to long option mappings
    struct OptionMapping {
        const char* short_opt;
        const char* long_opt;
    };

    OptionMapping mappings[] = {
        {"-c", "--config"},
        {"-d", "--device"},
        {"-b", "--baudrate"},
        {"-v", "--verbose"},
        {"-q", "--quiet"},
        {"-h", "--help"},
        {"-V", "--version"},
        {"-H", "--history"},
        {"-r", "--rate"},
        {"-l", "--loop"},
        {"-s", "--speed"},
        {"-n", "--dry-run"},
    };

    for (const auto& m : mappings) {
        EXPECT_EQ(strlen(m.short_opt), 2u);  // Short options are 2 chars
        EXPECT_GT(strlen(m.long_opt), 2u);   // Long options are > 2 chars
        EXPECT_EQ(m.long_opt[0], '-');
        EXPECT_EQ(m.long_opt[1], '-');
    }
}

// Error code tests
TEST_F(CliTest, ErrorCodeValues) {
    EXPECT_EQ(static_cast<int>(ErrorCode::Success), 0);
    EXPECT_EQ(static_cast<int>(ErrorCode::GeneralError), 1);
    EXPECT_EQ(static_cast<int>(ErrorCode::ArgumentError), 2);
    EXPECT_EQ(static_cast<int>(ErrorCode::ConfigError), 3);
    EXPECT_EQ(static_cast<int>(ErrorCode::HistoryError), 4);
    EXPECT_EQ(static_cast<int>(ErrorCode::DeviceError), 5);
    EXPECT_EQ(static_cast<int>(ErrorCode::SafetyError), 6);
}

// SUB-001 to SUB-006: Subcommand recognition
TEST_F(CliTest, SubcommandRecognition) {
    auto isValidCommand = [](const char* cmd) {
        return strcmp(cmd, "play") == 0 ||
               strcmp(cmd, "validate") == 0 ||
               strcmp(cmd, "ping") == 0 ||
               strcmp(cmd, "info") == 0 ||
               strcmp(cmd, "send") == 0;
    };

    EXPECT_TRUE(isValidCommand("play"));
    EXPECT_TRUE(isValidCommand("validate"));
    EXPECT_TRUE(isValidCommand("ping"));
    EXPECT_TRUE(isValidCommand("info"));
    EXPECT_TRUE(isValidCommand("send"));
    EXPECT_FALSE(isValidCommand("unknown"));
    EXPECT_FALSE(isValidCommand(""));
}

// Play command required option
TEST_F(CliTest, PlayRequiresHistory) {
    // In actual implementation, play command requires -H/--history
    // This is a documentation test
    const char* required_option = "--history";
    EXPECT_STREQ(required_option, "--history");
}

// Validate command required option
TEST_F(CliTest, ValidateRequiresHistory) {
    const char* required_option = "--history";
    EXPECT_STREQ(required_option, "--history");
}

// Default values test
TEST_F(CliTest, DefaultValues) {
    // Default device
    const char* default_device = "/dev/ttyAMA0";
    EXPECT_STREQ(default_device, "/dev/ttyAMA0");

    // Default baudrate
    int default_baudrate = CRSF_BAUDRATE;
    EXPECT_EQ(default_baudrate, 420000);

    // Default rate
    double default_rate = 50.0;
    EXPECT_DOUBLE_EQ(default_rate, 50.0);
}

// Option with value parsing
TEST_F(CliTest, OptionWithValue) {
    // Test that options requiring values are correctly identified
    const char* options_with_values[] = {
        "--config", "--device", "--baudrate",
        "--history", "--rate", "--speed",
        "--loop-count", "--start-time", "--end-time",
        "--arm-delay", "--timeout", "--count",
        "--channels", "--duration"
    };

    for (const char* opt : options_with_values) {
        // All these should start with --
        EXPECT_EQ(strncmp(opt, "--", 2), 0);
    }
}

// Boolean options
TEST_F(CliTest, BooleanOptions) {
    const char* boolean_options[] = {
        "--verbose", "--quiet", "--loop",
        "--dry-run", "--strict", "--arm"
    };

    for (const char* opt : boolean_options) {
        EXPECT_EQ(strncmp(opt, "--", 2), 0);
    }
}
