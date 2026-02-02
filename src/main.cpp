#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

#include "config/config.hpp"
#include "crsf/crsf.hpp"
#include "gpio/gpio_uart_map.hpp"
#include "history/history_loader.hpp"
#include "playback/playback_controller.hpp"
#include "safety/safety_monitor.hpp"
#include "uart/uart.hpp"

using namespace elrs;

// Version
constexpr const char* VERSION = "0.1.0";

// Print help
void printHelp(const char* program) {
    std::cout << "ExpressLRS Sender v" << VERSION << "\n\n"
        << "Usage: " << program << " [options] <command> [command-options]\n\n"
        << "Global Options:\n"
        << "  -c, --config <file>    Config file (default: config/default.json)\n"
        << "  -d, --device <path>    UART device (default: /dev/ttyAMA0)\n"
        << "  -g, --gpio <pin>       GPIO TX pin number (auto-resolves UART device)\n"
        << "  -b, --baudrate <bps>   Baudrate (default: 921600)\n"
        << "  -v, --verbose          Verbose output\n"
        << "  -q, --quiet            Quiet mode (errors only)\n"
        << "  -h, --help             Show this help\n"
        << "  -V, --version          Show version\n\n"
        << "Commands:\n"
        << "  play       Play recorded control history\n"
        << "  validate   Validate history file\n"
        << "  ping       Ping TX module\n"
        << "  info       Show device info\n"
        << "  send       Send single command\n"
        << "  gpio       Show GPIO-UART mapping table\n\n"
        << "Run '" << program << " <command> --help' for command-specific options.\n";
}

void printPlayHelp(const char* program) {
    std::cout << "Usage: " << program << " play [options] -H <file>\n\n"
        << "Options:\n"
        << "  -H, --history <file>   History file to play (required)\n"
        << "  -r, --rate <hz>        Packet rate (default: 500)\n"
        << "  -l, --loop             Loop playback\n"
        << "  --loop-count <n>       Number of loops (0=infinite)\n"
        << "  --start-time <ms>      Start position\n"
        << "  --end-time <ms>        End position\n"
        << "  -s, --speed <factor>   Speed multiplier (default: 1.0)\n"
        << "  -n, --dry-run          Don't actually send\n"
        << "  --arm-delay <ms>       Arm delay (default: 3000)\n";
}

void printValidateHelp(const char* program) {
    std::cout << "Usage: " << program << " validate [options] -H <file>\n\n"
        << "Options:\n"
        << "  -H, --history <file>   History file to validate (required)\n"
        << "  --strict               Treat warnings as errors\n";
}

// Setup logging
void setupLogging(const std::string& level, const std::string& log_file) {
    spdlog::level::level_enum log_level = spdlog::level::info;

    if (level == "trace") log_level = spdlog::level::trace;
    else if (level == "debug") log_level = spdlog::level::debug;
    else if (level == "info") log_level = spdlog::level::info;
    else if (level == "warn") log_level = spdlog::level::warn;
    else if (level == "error") log_level = spdlog::level::err;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_level(log_level);

    std::vector<spdlog::sink_ptr> sinks{console_sink};

    if (!log_file.empty()) {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(log_file, true);
        file_sink->set_level(spdlog::level::trace);
        sinks.push_back(file_sink);
    }

    auto logger = std::make_shared<spdlog::logger>("elrs", sinks.begin(), sinks.end());
    logger->set_level(spdlog::level::trace);
    spdlog::set_default_logger(logger);
}

// Command: play
int cmdPlay(config::AppConfig& config, int argc, char* argv[]) {
    std::string history_file;
    bool dry_run = false;

    // Parse play-specific arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--history") == 0) {
            if (i + 1 < argc) history_file = argv[++i];
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--rate") == 0) {
            if (i + 1 < argc) config.playback.rate_hz = std::stod(argv[++i]);
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--loop") == 0) {
            config.playback.loop = true;
        } else if (strcmp(argv[i], "--loop-count") == 0) {
            if (i + 1 < argc) config.playback.loop_count = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--start-time") == 0) {
            if (i + 1 < argc) config.playback.start_time_ms = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--end-time") == 0) {
            if (i + 1 < argc) config.playback.end_time_ms = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--speed") == 0) {
            if (i + 1 < argc) config.playback.speed = std::stod(argv[++i]);
        } else if (strcmp(argv[i], "-n") == 0 || strcmp(argv[i], "--dry-run") == 0) {
            dry_run = true;
        } else if (strcmp(argv[i], "--arm-delay") == 0) {
            if (i + 1 < argc) config.playback.arm_delay_ms = std::stoul(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0) {
            printPlayHelp("expresslrs_sender");
            return 0;
        }
    }

    if (history_file.empty()) {
        spdlog::error("History file is required (-H)");
        return static_cast<int>(ErrorCode::ArgumentError);
    }

    // Load history
    history::HistoryLoader loader;
    auto load_result = loader.load(history_file);
    if (!load_result.ok()) {
        spdlog::error("Failed to load history: {}", load_result.message);
        return static_cast<int>(load_result.error);
    }

    auto& frames = load_result.value;
    const auto& metadata = loader.getMetadata();

    spdlog::info("Loaded {} frames from {} ({:.1f}s, {:.1f}Hz)",
        metadata.frame_count, history_file,
        metadata.duration_ms / 1000.0, metadata.packet_rate_hz);

    // Validate
    auto validation = loader.validate(frames, false);
    for (const auto& warn : validation.warnings) {
        spdlog::warn("{}", warn);
    }
    if (!validation.valid) {
        for (const auto& err : validation.errors) {
            spdlog::error("{}", err);
        }
        return static_cast<int>(ErrorCode::HistoryError);
    }

    // Setup safety monitor
    safety::SafetyMonitor safety_monitor;
    safety_monitor.setConfig(config.safety);
    safety::SafetyMonitor::installSignalHandlers(&safety_monitor);

    // Setup UART (unless dry-run)
    uart::UartDriver uart;
    if (!dry_run) {
        uart::UartOptions uart_opts;
        uart_opts.baudrate = config.baudrate;
        uart_opts.half_duplex = config.half_duplex;

        auto uart_result = uart.open(config.device_port, uart_opts);
        if (!uart_result.ok()) {
            spdlog::error("Failed to open UART: {}", uart_result.message);
            return static_cast<int>(uart_result.error);
        }
        spdlog::info("Opened {} at {} baud{}", config.device_port, config.baudrate,
            config.half_duplex ? " (half-duplex)" : "");
    } else {
        spdlog::info("Dry-run mode - not sending to device");
    }

    // Setup playback controller
    playback::PlaybackController playback;
    playback.setFrames(std::move(frames));
    playback.setOptions(config.playback);

    // Frame send callback
    playback.setFrameCallback([&](const ChannelData& channels) -> bool {
        // Check for shutdown
        if (safety::SafetyMonitor::isShutdownRequested()) {
            return false;
        }

        // Process through safety
        ChannelData safe_channels = channels;
        safety_monitor.processChannels(safe_channels);

        // Build and send frame
        auto frame = crsf::buildRcChannelsFrame(safe_channels);

        if (!dry_run) {
            auto write_result = uart.write(frame);
            if (!write_result.ok()) {
                spdlog::error("UART write failed: {}", write_result.message);
                return false;
            }
        }

        safety_monitor.notifyFrameSent();
        return true;
    });

    // Start playback
    spdlog::info("Starting playback at {:.1f}Hz (speed {:.1f}x){}",
        config.playback.rate_hz, config.playback.speed,
        config.playback.loop ? " [LOOP]" : "");

    playback.start();

    // Main loop
    while (!playback.isComplete() && !safety::SafetyMonitor::isShutdownRequested()) {
        playback.tick();
        safety_monitor.checkFailsafe();

        // Small sleep to prevent busy-waiting
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Emergency stop handling - send disarm frames
    if (safety::SafetyMonitor::isShutdownRequested() && !dry_run) {
        spdlog::info("Sending {} disarm frames...", config.safety.disarm_frames);
        auto disarm_channels = safety_monitor.getFailsafeChannels();
        auto disarm_frame = crsf::buildRcChannelsFrame(disarm_channels);

        for (int i = 0; i < config.safety.disarm_frames; i++) {
            uart.write(disarm_frame);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    // Print stats
    auto stats = playback.getStats();
    spdlog::info("Playback complete: {} frames, {} loops, {:.1f}s, {:.1f}Hz actual, {:.1f}us jitter",
        stats.frames_sent, stats.loops_completed,
        stats.elapsed_ms / 1000.0, stats.actual_rate_hz, stats.timing_jitter_us);

    return safety::SafetyMonitor::isShutdownRequested() ? 130 : 0;
}

// Command: validate
int cmdValidate(config::AppConfig& config, int argc, char* argv[]) {
    (void)config;
    std::string history_file;
    bool strict = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--history") == 0) {
            if (i + 1 < argc) history_file = argv[++i];
        } else if (strcmp(argv[i], "--strict") == 0) {
            strict = true;
        } else if (strcmp(argv[i], "--help") == 0) {
            printValidateHelp("expresslrs_sender");
            return 0;
        }
    }

    if (history_file.empty()) {
        spdlog::error("History file is required (-H)");
        return static_cast<int>(ErrorCode::ArgumentError);
    }

    history::HistoryLoader loader;
    auto load_result = loader.load(history_file);
    if (!load_result.ok()) {
        std::cout << "Validating: " << history_file << "\n";
        std::cout << "Result: INVALID - " << load_result.message << "\n";
        return static_cast<int>(load_result.error);
    }

    const auto& metadata = loader.getMetadata();
    auto validation = loader.validate(load_result.value, strict);

    std::cout << "Validating: " << history_file << "\n"
        << "  Format: " << metadata.format << "\n"
        << "  Frames: " << metadata.frame_count << "\n"
        << "  Duration: " << (metadata.duration_ms / 1000.0) << "s\n"
        << "  Channels: " << metadata.channel_count << "\n"
        << "  Rate: " << metadata.packet_rate_hz << "Hz\n";

    if (!validation.warnings.empty()) {
        std::cout << "  Warnings:\n";
        for (const auto& warn : validation.warnings) {
            std::cout << "    - " << warn << "\n";
        }
    }

    if (!validation.errors.empty()) {
        std::cout << "  Errors:\n";
        for (const auto& err : validation.errors) {
            std::cout << "    - " << err << "\n";
        }
    }

    std::cout << "Result: " << (validation.valid ? "VALID" : "INVALID") << "\n";

    return validation.valid ? 0 : static_cast<int>(ErrorCode::HistoryError);
}

// Read a complete CRSF frame from UART with timeout
bool readCrsfFrame(uart::UartDriver& uart, std::vector<uint8_t>& frame_out, int timeout_ms) {
    std::vector<uint8_t> buffer;
    buffer.reserve(CRSF_MAX_FRAME_SIZE * 2);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        int read_timeout = std::max(1, static_cast<int>(remaining.count()));

        auto read_result = uart.read(CRSF_MAX_FRAME_SIZE, read_timeout);
        if (read_result.ok() && !read_result.value.empty()) {
            buffer.insert(buffer.end(), read_result.value.begin(), read_result.value.end());
        }

        // Try to extract a frame from accumulated data
        if (!buffer.empty()) {
            size_t consumed = crsf::extractFrame(buffer.data(), buffer.size(), frame_out);
            if (!frame_out.empty()) {
                return true;
            }
            // Remove consumed garbage bytes
            if (consumed > 0 && consumed <= buffer.size()) {
                buffer.erase(buffer.begin(), buffer.begin() + static_cast<ptrdiff_t>(consumed));
            }
        }
    }

    return false;
}

// Command: ping
int cmdPing(config::AppConfig& config, int argc, char* argv[]) {
    int timeout_ms = 1000;
    int count = 3;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 < argc) timeout_ms = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--count") == 0) {
            if (i + 1 < argc) count = std::stoi(argv[++i]);
        }
    }

    uart::UartDriver uart;
    uart::UartOptions uart_opts;
    uart_opts.baudrate = config.baudrate;
    uart_opts.half_duplex = config.half_duplex;

    auto uart_result = uart.open(config.device_port, uart_opts);
    if (!uart_result.ok()) {
        spdlog::error("Failed to open UART: {}", uart_result.message);
        return static_cast<int>(uart_result.error);
    }

    std::cout << "Pinging ELRS TX on " << config.device_port << "...\n";

    auto ping_frame = crsf::buildDevicePingFrame();
    int received = 0;
    double total_time = 0;

    for (int i = 0; i < count; i++) {
        auto start = std::chrono::steady_clock::now();

        auto write_result = uart.write(ping_frame);
        if (!write_result.ok()) {
            std::cout << "Send failed\n";
            continue;
        }

        std::vector<uint8_t> response;
        bool got_frame = readCrsfFrame(uart, response, timeout_ms);
        auto end = std::chrono::steady_clock::now();

        if (got_frame) {
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double ms = elapsed.count() / 1000.0;

            uint8_t frame_type = crsf::getFrameType(response.data(), response.size());
            if (frame_type == CRSF_FRAME_TYPE_DEVICE_INFO) {
                auto info = crsf::parseDeviceInfoFrame(response.data(), response.size());
                if (info) {
                    std::cout << "Response from " << info->device_name
                              << ": time=" << ms << "ms\n";
                } else {
                    std::cout << "Response (DEVICE_INFO, parse failed): time=" << ms << "ms\n";
                }
            } else {
                std::cout << "Response (type=0x" << std::hex << static_cast<int>(frame_type)
                          << std::dec << "): time=" << ms << "ms\n";
            }
            received++;
            total_time += ms;
        } else {
            std::cout << "Timeout\n";
        }

        if (i < count - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    std::cout << "--- ping statistics ---\n"
        << count << " packets transmitted, " << received << " received, "
        << ((count - received) * 100 / count) << "% packet loss\n";

    if (received > 0) {
        std::cout << "rtt avg = " << (total_time / received) << " ms\n";
    }

    return received > 0 ? 0 : static_cast<int>(ErrorCode::DeviceError);
}

// Command: info
int cmdInfo(config::AppConfig& config, int argc, char* argv[]) {
    int timeout_ms = 2000;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--timeout") == 0) {
            if (i + 1 < argc) timeout_ms = std::stoi(argv[++i]);
        }
    }

    uart::UartDriver uart;
    uart::UartOptions uart_opts;
    uart_opts.baudrate = config.baudrate;
    uart_opts.half_duplex = config.half_duplex;

    auto uart_result = uart.open(config.device_port, uart_opts);
    if (!uart_result.ok()) {
        spdlog::error("Failed to open UART: {}", uart_result.message);
        return static_cast<int>(uart_result.error);
    }

    std::cout << "Querying device info on " << config.device_port << "...\n";

    // Send DEVICE_PING to elicit DEVICE_INFO response
    auto ping_frame = crsf::buildDevicePingFrame();
    auto write_result = uart.write(ping_frame);
    if (!write_result.ok()) {
        spdlog::error("Failed to send ping: {}", write_result.message);
        return static_cast<int>(ErrorCode::DeviceError);
    }

    // Read response
    std::vector<uint8_t> response;
    bool got_frame = readCrsfFrame(uart, response, timeout_ms);

    if (!got_frame) {
        spdlog::error("No response from device (timeout {}ms)", timeout_ms);
        return static_cast<int>(ErrorCode::DeviceError);
    }

    uint8_t frame_type = crsf::getFrameType(response.data(), response.size());
    if (frame_type != CRSF_FRAME_TYPE_DEVICE_INFO) {
        spdlog::error("Unexpected response type: 0x{:02X} (expected DEVICE_INFO 0x{:02X})",
                       frame_type, CRSF_FRAME_TYPE_DEVICE_INFO);
        return static_cast<int>(ErrorCode::DeviceError);
    }

    auto info = crsf::parseDeviceInfoFrame(response.data(), response.size());
    if (!info) {
        spdlog::error("Failed to parse DEVICE_INFO response");
        return static_cast<int>(ErrorCode::DeviceError);
    }

    // Format serial/hw/fw as hex strings
    auto formatHex = [](const std::array<uint8_t, 4>& arr) -> std::string {
        char buf[12];
        snprintf(buf, sizeof(buf), "%02X%02X%02X%02X", arr[0], arr[1], arr[2], arr[3]);
        return std::string(buf);
    };

    std::cout << "Device: " << config.device_port << "\n"
        << "Baudrate: " << config.baudrate << "\n"
        << "Protocol: CRSF\n"
        << "Device Name: " << info->device_name << "\n"
        << "Serial: " << formatHex(info->serial_number) << "\n"
        << "Hardware ID: " << formatHex(info->hardware_id) << "\n"
        << "Firmware ID: " << formatHex(info->firmware_id) << "\n"
        << "Parameters: " << static_cast<int>(info->parameter_count) << "\n"
        << "Parameter Protocol: " << static_cast<int>(info->parameter_protocol_version) << "\n";

    return 0;
}

// Command: send
int cmdSend(config::AppConfig& config, int argc, char* argv[]) {
    ChannelData channels;
    channels.fill(CRSF_CHANNEL_MID);
    channels[2] = CRSF_CHANNEL_MIN;  // Throttle min by default

    int duration_ms = 1000;
    bool arm = false;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--channels") == 0) {
            if (i + 1 < argc) {
                std::string ch_str = argv[++i];
                std::istringstream ss(ch_str);
                std::string token;
                size_t ch = 0;
                while (std::getline(ss, token, ',') && ch < CRSF_MAX_CHANNELS) {
                    channels[ch++] = static_cast<int16_t>(std::stoi(token));
                }
            }
        } else if (strcmp(argv[i], "--duration") == 0) {
            if (i + 1 < argc) duration_ms = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "--arm") == 0) {
            arm = true;
        }
    }

    if (arm) {
        channels[config.safety.arm_channel] = CRSF_CHANNEL_MAX;
    }

    uart::UartDriver uart;
    uart::UartOptions uart_opts;
    uart_opts.baudrate = config.baudrate;
    uart_opts.half_duplex = config.half_duplex;

    auto uart_result = uart.open(config.device_port, uart_opts);
    if (!uart_result.ok()) {
        spdlog::error("Failed to open UART: {}", uart_result.message);
        return static_cast<int>(uart_result.error);
    }

    safety::SafetyMonitor safety_monitor;
    safety_monitor.setConfig(config.safety);
    safety::SafetyMonitor::installSignalHandlers(&safety_monitor);

    spdlog::info("Sending for {}ms{}...", duration_ms, arm ? " (ARMED)" : "");

    auto start = std::chrono::steady_clock::now();
    auto send_interval = std::chrono::milliseconds(2);  // 500Hz
    auto last_send = start;

    while (!safety::SafetyMonitor::isShutdownRequested()) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);

        if (elapsed.count() >= duration_ms) {
            break;
        }

        auto since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_send);
        if (since_last >= send_interval) {
            ChannelData safe_channels = channels;
            safety_monitor.processChannels(safe_channels);

            auto frame = crsf::buildRcChannelsFrame(safe_channels);
            uart.write(frame);
            safety_monitor.notifyFrameSent();

            last_send = now;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    // Send disarm
    auto disarm_channels = safety_monitor.getFailsafeChannels();
    auto disarm_frame = crsf::buildRcChannelsFrame(disarm_channels);
    for (int i = 0; i < 10; i++) {
        uart.write(disarm_frame);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    spdlog::info("Done");
    return 0;
}

// Command: gpio
int cmdGpio() {
    auto uarts = gpio::getAvailableUarts();

    std::cout << "Available UART-GPIO mappings (Raspberry Pi 4/5):\n\n"
        << "  UART   GPIO TX  GPIO RX  Device         Description\n"
        << "  -----  -------  -------  -------------  ---------------------------\n";

    for (const auto& info : uarts) {
        char line[128];
        snprintf(line, sizeof(line), "  UART%d  %-7d  %-7d  %-13s  %s",
                 info.uart_number, info.gpio_tx, info.gpio_rx,
                 info.device_path.c_str(), info.description.c_str());
        std::cout << line << "\n";
    }

    std::cout << "\nNote:\n"
        << "  - UART1 (mini UART) is excluded (unreliable at 921600 baud)\n"
        << "  - UART2 (GPIO0/1) is shared with I2C0\n"
        << "  - UART4 (GPIO8/9) is shared with SPI0 CE0/CE1\n"
        << "  - Enable additional UARTs in /boot/config.txt:\n"
        << "      dtoverlay=uart3\n"
        << "      dtoverlay=uart4\n"
        << "      dtoverlay=uart5\n";

    return 0;
}

int main(int argc, char* argv[]) {
    // Default configuration
    config::AppConfig config = config::getDefaultConfig();
    std::string config_file;
    std::string log_level = "info";
    std::string command;
    int cmd_argc = 0;
    char** cmd_argv = nullptr;

    // CLI overrides (applied after config file loading)
    std::string cli_device;
    int cli_gpio_tx = -1;
    int cli_baudrate = -1;

    // Parse global arguments
    int i = 1;
    while (i < argc) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--config") == 0) {
            if (i + 1 < argc) config_file = argv[++i];
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--device") == 0) {
            if (i + 1 < argc) cli_device = argv[++i];
        } else if (strcmp(argv[i], "-g") == 0 || strcmp(argv[i], "--gpio") == 0) {
            if (i + 1 < argc) cli_gpio_tx = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--baudrate") == 0) {
            if (i + 1 < argc) cli_baudrate = std::stoi(argv[++i]);
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            log_level = "debug";
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            log_level = "error";
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printHelp(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-V") == 0 || strcmp(argv[i], "--version") == 0) {
            std::cout << "ExpressLRS Sender v" << VERSION << "\n";
            return 0;
        } else if (argv[i][0] != '-') {
            // This is the command
            command = argv[i];
            cmd_argc = argc - i - 1;
            cmd_argv = &argv[i + 1];
            break;
        } else {
            std::cerr << "Unknown option: " << argv[i] << "\n";
            return static_cast<int>(ErrorCode::ArgumentError);
        }
        i++;
    }

    // Load config file if specified
    if (!config_file.empty()) {
        auto config_result = config::loadConfig(config_file);
        if (!config_result.ok()) {
            std::cerr << "Error loading config: " << config_result.message << "\n";
            return static_cast<int>(config_result.error);
        }
        config = config_result.value;
    }

    // Apply CLI overrides (take precedence over config file)
    if (!cli_device.empty()) {
        config.device_port = cli_device;
    }
    if (cli_gpio_tx >= 0) {
        config.gpio_tx = cli_gpio_tx;
        config.device_port = gpio::resolveDevicePath(std::to_string(cli_gpio_tx));
    }
    if (cli_baudrate >= 0) {
        config.baudrate = cli_baudrate;
    }

    // Setup logging
    setupLogging(log_level, config.log_file);

    // No command specified
    if (command.empty()) {
        printHelp(argv[0]);
        return static_cast<int>(ErrorCode::ArgumentError);
    }

    // Dispatch command
    if (command == "play") {
        return cmdPlay(config, cmd_argc, cmd_argv);
    } else if (command == "validate") {
        return cmdValidate(config, cmd_argc, cmd_argv);
    } else if (command == "ping") {
        return cmdPing(config, cmd_argc, cmd_argv);
    } else if (command == "info") {
        return cmdInfo(config, cmd_argc, cmd_argv);
    } else if (command == "send") {
        return cmdSend(config, cmd_argc, cmd_argv);
    } else if (command == "gpio") {
        return cmdGpio();
    } else {
        std::cerr << "Unknown command: " << command << "\n";
        printHelp(argv[0]);
        return static_cast<int>(ErrorCode::ArgumentError);
    }
}
