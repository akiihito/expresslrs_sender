#pragma once

#include <string>
#include <vector>

#include "expresslrs_sender/types.hpp"

namespace elrs {
namespace history {

// Metadata for loaded history
struct HistoryMetadata {
    std::string name;
    std::string format;        // "csv" or "json"
    uint32_t duration_ms;
    size_t frame_count;
    size_t channel_count;
    double packet_rate_hz;
};

// Validation result
struct ValidationResult {
    bool valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

class HistoryLoader {
public:
    HistoryLoader() = default;

    // Load history from file (auto-detect format)
    Result<std::vector<HistoryFrame>> load(const std::string& filepath);

    // Load specific format
    Result<std::vector<HistoryFrame>> loadCsv(const std::string& filepath);
    Result<std::vector<HistoryFrame>> loadJson(const std::string& filepath);

    // Validate loaded frames
    ValidationResult validate(const std::vector<HistoryFrame>& frames, bool strict = false);

    // Get metadata after loading
    const HistoryMetadata& getMetadata() const { return m_metadata; }

private:
    HistoryMetadata m_metadata;

    // Parse CSV line
    Result<HistoryFrame> parseCsvLine(const std::string& line, size_t line_num);

    // Detect file format from extension or content
    std::string detectFormat(const std::string& filepath);

    // Calculate metadata from frames
    void calculateMetadata(const std::vector<HistoryFrame>& frames, const std::string& format);
};

}  // namespace history
}  // namespace elrs
