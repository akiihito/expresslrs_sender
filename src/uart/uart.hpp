#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "expresslrs_sender/types.hpp"

namespace elrs {
namespace uart {

// UART options
struct UartOptions {
    int baudrate = CRSF_BAUDRATE;
    bool half_duplex = false; // 半二重通信モード（送信後 tcdrain で完了を保証）
};

class UartDriver {
public:
    UartDriver();
    ~UartDriver();

    // Disable copy
    UartDriver(const UartDriver&) = delete;
    UartDriver& operator=(const UartDriver&) = delete;

    // Enable move
    UartDriver(UartDriver&& other) noexcept;
    UartDriver& operator=(UartDriver&& other) noexcept;

    // Open serial port
    Result<void> open(const std::string& device, int baudrate = CRSF_BAUDRATE);
    Result<void> open(const std::string& device, const UartOptions& options);

    // Close serial port
    void close();

    // Check if port is open
    bool isOpen() const;

    // Write data
    Result<size_t> write(const uint8_t* data, size_t len);
    Result<size_t> write(const std::vector<uint8_t>& data);

    template <size_t N>
    Result<size_t> write(const std::array<uint8_t, N>& data) {
        return write(data.data(), N);
    }

    // Read data (with timeout in ms, 0 = non-blocking)
    Result<std::vector<uint8_t>> read(size_t max_len, int timeout_ms = 100);

    // Enable/disable TX (for half-duplex direction control)
    void setTxEnabled(bool enabled);

    // Flush buffers
    void flush();

    // Get file descriptor (for advanced use)
    int getFd() const { return m_fd; }

private:
    int m_fd;
    std::string m_device;
    UartOptions m_options;

    Result<void> configure(int baudrate);
};

}  // namespace uart
}  // namespace elrs
