#include "gpio_uart_map.hpp"

#include <algorithm>

namespace elrs {
namespace gpio {

// Raspberry Pi 4/5 UART-GPIO mapping table
// UART1 (mini UART) is excluded as it cannot reliably support 921600 baud
static const std::vector<GpioUartInfo> UART_MAP = {
    {14, 15, 0, "/dev/ttyAMA0", "UART0 (PL011) - default"},
    { 0,  1, 2, "/dev/ttyAMA1", "UART2 - shared with I2C0"},
    { 4,  5, 3, "/dev/ttyAMA2", "UART3"},
    { 8,  9, 4, "/dev/ttyAMA3", "UART4 - shared with SPI0 CE0/CE1"},
    {12, 13, 5, "/dev/ttyAMA4", "UART5"},
};

std::vector<GpioUartInfo> getAvailableUarts() {
    return UART_MAP;
}

std::optional<GpioUartInfo> findByGpioTx(int gpio_tx) {
    auto it = std::find_if(UART_MAP.begin(), UART_MAP.end(),
        [gpio_tx](const GpioUartInfo& info) {
            return info.gpio_tx == gpio_tx;
        });
    if (it != UART_MAP.end()) {
        return *it;
    }
    return std::nullopt;
}

std::optional<GpioUartInfo> findByUartNumber(int uart_number) {
    auto it = std::find_if(UART_MAP.begin(), UART_MAP.end(),
        [uart_number](const GpioUartInfo& info) {
            return info.uart_number == uart_number;
        });
    if (it != UART_MAP.end()) {
        return *it;
    }
    return std::nullopt;
}

std::string resolveDevicePath(const std::string& spec) {
    if (spec.empty()) {
        return spec;
    }

    // If it starts with '/' it's already a device path
    if (spec[0] == '/') {
        return spec;
    }

    // Try to parse as a GPIO pin number
    try {
        int gpio_pin = std::stoi(spec);
        auto info = findByGpioTx(gpio_pin);
        if (info) {
            return info->device_path;
        }
    } catch (const std::exception&) {
        // Not a number, return as-is
    }

    return spec;
}

}  // namespace gpio
}  // namespace elrs
