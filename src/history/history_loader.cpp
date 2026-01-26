#include "history_loader.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace elrs {
namespace history {

using json = nlohmann::json;

std::string HistoryLoader::detectFormat(const std::string& filepath) {
    // Check extension
    size_t dot_pos = filepath.rfind('.');
    if (dot_pos != std::string::npos) {
        std::string ext = filepath.substr(dot_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == "json") return "json";
        if (ext == "csv") return "csv";
    }

    // Try to detect from content
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return "";
    }

    std::string first_line;
    std::getline(file, first_line);

    // Check if it starts with '{'
    size_t first_char = first_line.find_first_not_of(" \t\n\r");
    if (first_char != std::string::npos && first_line[first_char] == '{') {
        return "json";
    }

    return "csv";
}

Result<std::vector<HistoryFrame>> HistoryLoader::load(const std::string& filepath) {
    std::string format = detectFormat(filepath);

    if (format == "json") {
        return loadJson(filepath);
    } else if (format == "csv") {
        return loadCsv(filepath);
    }

    return Result<std::vector<HistoryFrame>>::failure(
        ErrorCode::HistoryError,
        "Unknown file format: " + filepath
    );
}

Result<HistoryFrame> HistoryLoader::parseCsvLine(const std::string& line, size_t line_num) {
    HistoryFrame frame{};
    std::istringstream ss(line);
    std::string token;

    // Parse timestamp
    if (!std::getline(ss, token, ',')) {
        return Result<HistoryFrame>::failure(
            ErrorCode::HistoryError,
            "Line " + std::to_string(line_num) + ": Missing timestamp"
        );
    }

    try {
        frame.timestamp_ms = static_cast<uint32_t>(std::stoul(token));
    } catch (...) {
        return Result<HistoryFrame>::failure(
            ErrorCode::HistoryError,
            "Line " + std::to_string(line_num) + ": Invalid timestamp"
        );
    }

    // Parse channels
    size_t ch = 0;
    while (std::getline(ss, token, ',') && ch < CRSF_MAX_CHANNELS) {
        try {
            frame.channels[ch] = static_cast<int16_t>(std::stoi(token));
        } catch (...) {
            return Result<HistoryFrame>::failure(
                ErrorCode::HistoryError,
                "Line " + std::to_string(line_num) + ": Invalid channel value"
            );
        }
        ch++;
    }

    // Fill remaining channels with center value
    while (ch < CRSF_MAX_CHANNELS) {
        frame.channels[ch++] = CRSF_CHANNEL_MID;
    }

    return Result<HistoryFrame>::success(frame);
}

Result<std::vector<HistoryFrame>> HistoryLoader::loadCsv(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return Result<std::vector<HistoryFrame>>::failure(
            ErrorCode::HistoryError,
            "Cannot open file: " + filepath
        );
    }

    std::vector<HistoryFrame> frames;
    std::string line;
    size_t line_num = 0;
    bool header_skipped = false;

    while (std::getline(file, line)) {
        line_num++;

        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t\n\r") == std::string::npos) {
            continue;
        }

        // Skip header line (if it contains non-numeric first field)
        if (!header_skipped) {
            size_t first_comma = line.find(',');
            std::string first_field = (first_comma != std::string::npos)
                ? line.substr(0, first_comma)
                : line;

            bool is_header = false;
            for (char c : first_field) {
                if (!std::isdigit(c) && c != '-' && c != ' ' && c != '\t') {
                    is_header = true;
                    break;
                }
            }

            if (is_header) {
                header_skipped = true;
                continue;
            }
            header_skipped = true;
        }

        auto result = parseCsvLine(line, line_num);
        if (!result.ok()) {
            return Result<std::vector<HistoryFrame>>::failure(result.error, result.message);
        }

        frames.push_back(result.value);
    }

    if (frames.empty()) {
        return Result<std::vector<HistoryFrame>>::failure(
            ErrorCode::HistoryError,
            "No frames found in file"
        );
    }

    calculateMetadata(frames, "csv");

    return Result<std::vector<HistoryFrame>>::success(std::move(frames));
}

Result<std::vector<HistoryFrame>> HistoryLoader::loadJson(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return Result<std::vector<HistoryFrame>>::failure(
            ErrorCode::HistoryError,
            "Cannot open file: " + filepath
        );
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        return Result<std::vector<HistoryFrame>>::failure(
            ErrorCode::HistoryError,
            "JSON parse error: " + std::string(e.what())
        );
    }

    // Check for required fields
    if (!j.contains("frames") || !j["frames"].is_array()) {
        return Result<std::vector<HistoryFrame>>::failure(
            ErrorCode::HistoryError,
            "Missing 'frames' array in JSON"
        );
    }

    std::vector<HistoryFrame> frames;

    try {
        for (const auto& frame_json : j["frames"]) {
            HistoryFrame frame{};

            // Get timestamp
            if (frame_json.contains("t")) {
                frame.timestamp_ms = frame_json["t"].get<uint32_t>();
            } else if (frame_json.contains("timestamp_ms")) {
                frame.timestamp_ms = frame_json["timestamp_ms"].get<uint32_t>();
            } else {
                return Result<std::vector<HistoryFrame>>::failure(
                    ErrorCode::HistoryError,
                    "Missing timestamp in frame"
                );
            }

            // Get channels
            if (frame_json.contains("ch")) {
                const auto& ch_array = frame_json["ch"];
                for (size_t i = 0; i < ch_array.size() && i < CRSF_MAX_CHANNELS; i++) {
                    frame.channels[i] = ch_array[i].get<int16_t>();
                }
                // Fill remaining with center
                for (size_t i = ch_array.size(); i < CRSF_MAX_CHANNELS; i++) {
                    frame.channels[i] = CRSF_CHANNEL_MID;
                }
            } else if (frame_json.contains("channels")) {
                const auto& ch_array = frame_json["channels"];
                for (size_t i = 0; i < ch_array.size() && i < CRSF_MAX_CHANNELS; i++) {
                    frame.channels[i] = ch_array[i].get<int16_t>();
                }
                for (size_t i = ch_array.size(); i < CRSF_MAX_CHANNELS; i++) {
                    frame.channels[i] = CRSF_CHANNEL_MID;
                }
            } else {
                return Result<std::vector<HistoryFrame>>::failure(
                    ErrorCode::HistoryError,
                    "Missing channels in frame"
                );
            }

            frames.push_back(frame);
        }
    } catch (const json::exception& e) {
        return Result<std::vector<HistoryFrame>>::failure(
            ErrorCode::HistoryError,
            "JSON error: " + std::string(e.what())
        );
    }

    if (frames.empty()) {
        return Result<std::vector<HistoryFrame>>::failure(
            ErrorCode::HistoryError,
            "No frames found in file"
        );
    }

    // Extract metadata from JSON if present
    if (j.contains("metadata")) {
        const auto& meta = j["metadata"];
        if (meta.contains("name")) {
            m_metadata.name = meta["name"].get<std::string>();
        }
    }

    calculateMetadata(frames, "json");

    return Result<std::vector<HistoryFrame>>::success(std::move(frames));
}

ValidationResult HistoryLoader::validate(const std::vector<HistoryFrame>& frames, bool strict) {
    ValidationResult result{true, {}, {}};

    if (frames.empty()) {
        result.valid = false;
        result.errors.push_back("No frames to validate");
        return result;
    }

    uint32_t prev_timestamp = 0;
    bool first_frame = true;

    for (size_t i = 0; i < frames.size(); i++) {
        const auto& frame = frames[i];

        // Check timestamp ordering
        if (!first_frame) {
            if (frame.timestamp_ms < prev_timestamp) {
                result.valid = false;
                result.errors.push_back(
                    "Frame " + std::to_string(i) + ": Timestamp not monotonic (" +
                    std::to_string(frame.timestamp_ms) + " < " + std::to_string(prev_timestamp) + ")"
                );
            } else if (frame.timestamp_ms == prev_timestamp) {
                result.warnings.push_back(
                    "Frame " + std::to_string(i) + ": Duplicate timestamp " +
                    std::to_string(frame.timestamp_ms)
                );
            }
        }
        prev_timestamp = frame.timestamp_ms;
        first_frame = false;

        // Check channel values
        for (size_t ch = 0; ch < CRSF_MAX_CHANNELS; ch++) {
            int16_t val = frame.channels[ch];
            if (val < CRSF_CHANNEL_MIN || val > CRSF_CHANNEL_MAX) {
                std::string msg = "Frame " + std::to_string(i) + ", CH" + std::to_string(ch + 1) +
                    ": Value out of range (" + std::to_string(val) + ")";

                if (strict) {
                    result.valid = false;
                    result.errors.push_back(msg);
                } else {
                    result.warnings.push_back(msg);
                }
            }
        }
    }

    // In strict mode, warnings become errors
    if (strict && !result.warnings.empty()) {
        result.valid = false;
        for (const auto& warn : result.warnings) {
            result.errors.push_back(warn);
        }
        result.warnings.clear();
    }

    return result;
}

void HistoryLoader::calculateMetadata(const std::vector<HistoryFrame>& frames, const std::string& format) {
    m_metadata.format = format;
    m_metadata.frame_count = frames.size();

    if (frames.empty()) {
        m_metadata.duration_ms = 0;
        m_metadata.packet_rate_hz = 0;
        m_metadata.channel_count = 0;
        return;
    }

    // Calculate duration
    m_metadata.duration_ms = frames.back().timestamp_ms - frames.front().timestamp_ms;

    // Calculate average packet rate
    if (frames.size() > 1 && m_metadata.duration_ms > 0) {
        m_metadata.packet_rate_hz = static_cast<double>(frames.size() - 1) * 1000.0 /
            static_cast<double>(m_metadata.duration_ms);
    } else {
        m_metadata.packet_rate_hz = 0;
    }

    // Count active channels (non-center values)
    size_t active_channels = 0;
    for (size_t ch = 0; ch < CRSF_MAX_CHANNELS; ch++) {
        bool has_non_center = false;
        for (const auto& frame : frames) {
            if (frame.channels[ch] != CRSF_CHANNEL_MID) {
                has_non_center = true;
                break;
            }
        }
        if (has_non_center) {
            active_channels = ch + 1;
        }
    }
    m_metadata.channel_count = active_channels > 0 ? active_channels : 8;
}

}  // namespace history
}  // namespace elrs
