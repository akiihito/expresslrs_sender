#include <gtest/gtest.h>

#include <fstream>
#include <filesystem>

#include "history/history_loader.hpp"

using namespace elrs;
using namespace elrs::history;

class HistoryLoaderTest : public ::testing::Test {
protected:
    std::string test_dir;

    void SetUp() override {
        test_dir = std::filesystem::temp_directory_path() / "elrs_test";
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

// CSV-001: Normal CSV
TEST_F(HistoryLoaderTest, LoadValidCsv) {
    std::string content =
        "timestamp_ms,ch1,ch2,ch3,ch4,ch5,ch6,ch7,ch8\n"
        "0,992,992,172,992,172,172,172,172\n"
        "20,992,992,200,992,172,172,172,172\n"
        "40,992,992,250,992,172,172,172,172\n";

    auto path = createFile("valid.csv", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.size(), 3u);
    EXPECT_EQ(result.value[0].timestamp_ms, 0u);
    EXPECT_EQ(result.value[1].timestamp_ms, 20u);
    EXPECT_EQ(result.value[2].timestamp_ms, 40u);
}

// CSV-002: Header line skip
TEST_F(HistoryLoaderTest, CsvSkipsHeader) {
    std::string content =
        "time,a,b,c,d,e,f,g,h\n"
        "0,992,992,172,992,172,172,172,172\n";

    auto path = createFile("header.csv", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.size(), 1u);
}

// CSV-003: 8 channels
TEST_F(HistoryLoaderTest, Csv8Channels) {
    std::string content =
        "0,100,200,300,400,500,600,700,800\n";

    auto path = createFile("8ch.csv", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value[0].channels[0], 100);
    EXPECT_EQ(result.value[0].channels[7], 800);
    // Remaining channels should be center
    EXPECT_EQ(result.value[0].channels[8], CRSF_CHANNEL_MID);
}

// CSV-005: Empty file
TEST_F(HistoryLoaderTest, CsvEmptyFile) {
    auto path = createFile("empty.csv", "");

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, ErrorCode::HistoryError);
}

// CSV-006: Invalid format (wrong column count)
TEST_F(HistoryLoaderTest, CsvInvalidFormat) {
    // Just header, no data
    std::string content = "timestamp_ms,ch1,ch2\n";

    auto path = createFile("invalid.csv", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_FALSE(result.ok());
}

// CSV-007: Non-numeric value
TEST_F(HistoryLoaderTest, CsvNonNumeric) {
    std::string content =
        "0,992,abc,172,992,172,172,172,172\n";

    auto path = createFile("nonnumeric.csv", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_FALSE(result.ok());
}

// JSON-001: Normal JSON
TEST_F(HistoryLoaderTest, LoadValidJson) {
    std::string content = R"({
        "metadata": {"name": "test"},
        "frames": [
            {"t": 0, "ch": [992, 992, 172, 992, 172, 172, 172, 172]},
            {"t": 20, "ch": [992, 992, 200, 992, 172, 172, 172, 172]}
        ]
    })";

    auto path = createFile("valid.json", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.value.size(), 2u);
    EXPECT_EQ(result.value[0].timestamp_ms, 0u);
    EXPECT_EQ(result.value[1].channels[2], 200);
}

// JSON-002: Metadata extraction
TEST_F(HistoryLoaderTest, JsonMetadata) {
    std::string content = R"({
        "metadata": {"name": "flight_001", "duration_ms": 5000},
        "frames": [
            {"t": 0, "ch": [992, 992, 172, 992]},
            {"t": 100, "ch": [992, 992, 172, 992]}
        ]
    })";

    auto path = createFile("meta.json", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(loader.getMetadata().name, "flight_001");
}

// JSON-004: Invalid JSON syntax
TEST_F(HistoryLoaderTest, JsonInvalidSyntax) {
    std::string content = R"({ invalid json )";

    auto path = createFile("invalid.json", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, ErrorCode::HistoryError);
}

// JSON-005: Missing frames field
TEST_F(HistoryLoaderTest, JsonMissingFrames) {
    std::string content = R"({"metadata": {}})";

    auto path = createFile("noframes.json", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_FALSE(result.ok());
}

// VAL-001: Value below range
TEST_F(HistoryLoaderTest, ValidateBelowRange) {
    std::vector<HistoryFrame> frames = {
        {0, {100, 992, 172, 992, 172, 172, 172, 172, 992, 992, 992, 992, 992, 992, 992, 992}}
    };

    HistoryLoader loader;
    auto result = loader.validate(frames, false);

    EXPECT_TRUE(result.valid);  // Warning only in non-strict mode
    EXPECT_FALSE(result.warnings.empty());
}

// VAL-002: Values in range
TEST_F(HistoryLoaderTest, ValidateInRange) {
    std::vector<HistoryFrame> frames = {
        {0, {992, 992, 172, 992, 172, 172, 172, 172, 992, 992, 992, 992, 992, 992, 992, 992}},
        {20, {992, 992, 500, 992, 172, 172, 172, 172, 992, 992, 992, 992, 992, 992, 992, 992}}
    };

    HistoryLoader loader;
    auto result = loader.validate(frames, false);

    EXPECT_TRUE(result.valid);
    EXPECT_TRUE(result.errors.empty());
}

// VAL-003: Timestamp ascending
TEST_F(HistoryLoaderTest, ValidateTimestampAscending) {
    std::vector<HistoryFrame> frames = {
        {0, {}},
        {20, {}},
        {40, {}}
    };

    HistoryLoader loader;
    auto result = loader.validate(frames, false);

    EXPECT_TRUE(result.valid);
}

// VAL-004: Timestamp descending
TEST_F(HistoryLoaderTest, ValidateTimestampDescending) {
    std::vector<HistoryFrame> frames = {
        {0, {}},
        {40, {}},
        {20, {}}  // Out of order
    };

    HistoryLoader loader;
    auto result = loader.validate(frames, false);

    EXPECT_FALSE(result.valid);
    EXPECT_FALSE(result.errors.empty());
}

// VAL-005: Duplicate timestamp
TEST_F(HistoryLoaderTest, ValidateDuplicateTimestamp) {
    std::vector<HistoryFrame> frames = {
        {0, {}},
        {20, {}},
        {20, {}}  // Duplicate
    };

    HistoryLoader loader;
    auto result = loader.validate(frames, false);

    EXPECT_TRUE(result.valid);  // Warning only
    EXPECT_FALSE(result.warnings.empty());
}

// Strict mode test
TEST_F(HistoryLoaderTest, ValidateStrictMode) {
    std::vector<HistoryFrame> frames = {
        {0, {100, 992, 172, 992, 172, 172, 172, 172, 992, 992, 992, 992, 992, 992, 992, 992}}
    };

    HistoryLoader loader;
    auto result = loader.validate(frames, true);

    EXPECT_FALSE(result.valid);  // Warnings become errors in strict mode
}

// File not found
TEST_F(HistoryLoaderTest, FileNotFound) {
    HistoryLoader loader;
    auto result = loader.load("/nonexistent/file.csv");

    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.error, ErrorCode::HistoryError);
}

// Auto-detect format
TEST_F(HistoryLoaderTest, AutoDetectCsv) {
    std::string content = "0,992,992,172,992,172,172,172,172\n";
    auto path = createFile("test.csv", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(loader.getMetadata().format, "csv");
}

TEST_F(HistoryLoaderTest, AutoDetectJson) {
    std::string content = R"({"frames": [{"t": 0, "ch": [992]}]})";
    auto path = createFile("test.json", content);

    HistoryLoader loader;
    auto result = loader.load(path);

    EXPECT_TRUE(result.ok());
    EXPECT_EQ(loader.getMetadata().format, "json");
}
