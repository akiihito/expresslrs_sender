#pragma once

#include <optional>
#include <string>
#include <vector>

namespace elrs {
namespace gpio {

struct GpioUartInfo {
    int gpio_tx;             // GPIO TX pin number
    int gpio_rx;             // GPIO RX pin number
    int uart_number;         // UART number (0-5)
    std::string device_path; // /dev/ttyAMAx
    std::string description; // Description
};

// Return all available UART-GPIO mappings for Pi 4/5
std::vector<GpioUartInfo> getAvailableUarts();

// Find UART info by GPIO TX pin number
std::optional<GpioUartInfo> findByGpioTx(int gpio_tx);

// Find UART info by UART number
std::optional<GpioUartInfo> findByUartNumber(int uart_number);

// Resolve device path from GPIO pin or device path string
// - Numeric string ("14") -> resolve as GPIO TX pin
// - Path string ("/dev/ttyAMA0") -> return as-is
// - Unknown GPIO number -> return original string as-is
std::string resolveDevicePath(const std::string& spec);

}  // namespace gpio
}  // namespace elrs
