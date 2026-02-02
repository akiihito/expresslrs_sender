#include <gtest/gtest.h>

#include "gpio/gpio_uart_map.hpp"

using namespace elrs::gpio;

class GpioUartMapTest : public ::testing::Test {};

// GPIO-001: getAvailableUarts returns 5 entries
TEST_F(GpioUartMapTest, GetAvailableUartsReturns5Entries) {
    auto uarts = getAvailableUarts();
    EXPECT_EQ(uarts.size(), 5u);
}

// GPIO-002: findByGpioTx(14) -> UART0, /dev/ttyAMA0
TEST_F(GpioUartMapTest, FindByGpioTx14) {
    auto info = findByGpioTx(14);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->uart_number, 0);
    EXPECT_EQ(info->device_path, "/dev/ttyAMA0");
    EXPECT_EQ(info->gpio_rx, 15);
}

// GPIO-003: findByGpioTx(4) -> UART3, /dev/ttyAMA3
TEST_F(GpioUartMapTest, FindByGpioTx4) {
    auto info = findByGpioTx(4);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->uart_number, 3);
    EXPECT_EQ(info->device_path, "/dev/ttyAMA3");
    EXPECT_EQ(info->gpio_rx, 5);
}

// GPIO-004: findByGpioTx(99) -> nullopt (invalid GPIO)
TEST_F(GpioUartMapTest, FindByGpioTxInvalid) {
    auto info = findByGpioTx(99);
    EXPECT_FALSE(info.has_value());
}

// GPIO-005: findByUartNumber(0) -> GPIO14/15
TEST_F(GpioUartMapTest, FindByUartNumber0) {
    auto info = findByUartNumber(0);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->gpio_tx, 14);
    EXPECT_EQ(info->gpio_rx, 15);
    EXPECT_EQ(info->device_path, "/dev/ttyAMA0");
}

// GPIO-006: findByUartNumber(1) -> nullopt (mini UART excluded)
TEST_F(GpioUartMapTest, FindByUartNumber1Excluded) {
    auto info = findByUartNumber(1);
    EXPECT_FALSE(info.has_value());
}

// GPIO-007: findByUartNumber(3) -> GPIO4/5, /dev/ttyAMA3
TEST_F(GpioUartMapTest, FindByUartNumber3) {
    auto info = findByUartNumber(3);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->gpio_tx, 4);
    EXPECT_EQ(info->gpio_rx, 5);
    EXPECT_EQ(info->device_path, "/dev/ttyAMA3");
}

// GPIO-008: resolveDevicePath("14") -> /dev/ttyAMA0
TEST_F(GpioUartMapTest, ResolveDevicePathGpio14) {
    EXPECT_EQ(resolveDevicePath("14"), "/dev/ttyAMA0");
}

// GPIO-009: resolveDevicePath("4") -> /dev/ttyAMA3
TEST_F(GpioUartMapTest, ResolveDevicePathGpio4) {
    EXPECT_EQ(resolveDevicePath("4"), "/dev/ttyAMA3");
}

// GPIO-010: resolveDevicePath("/dev/ttyUSB0") -> passthrough
TEST_F(GpioUartMapTest, ResolveDevicePathPassthrough) {
    EXPECT_EQ(resolveDevicePath("/dev/ttyUSB0"), "/dev/ttyUSB0");
}

// GPIO-011: resolveDevicePath("99") -> returns original (unknown GPIO)
TEST_F(GpioUartMapTest, ResolveDevicePathUnknownGpio) {
    EXPECT_EQ(resolveDevicePath("99"), "99");
}

// GPIO-012: resolveDevicePath("") -> returns empty string
TEST_F(GpioUartMapTest, ResolveDevicePathEmpty) {
    EXPECT_EQ(resolveDevicePath(""), "");
}

// GPIO-013: All UART entries have valid fields
TEST_F(GpioUartMapTest, AllEntriesHaveValidFields) {
    auto uarts = getAvailableUarts();
    for (const auto& info : uarts) {
        EXPECT_GE(info.gpio_tx, 0);
        EXPECT_GE(info.gpio_rx, 0);
        EXPECT_GE(info.uart_number, 0);
        EXPECT_FALSE(info.device_path.empty());
        EXPECT_FALSE(info.description.empty());
    }
}

// GPIO-014: findByGpioTx for all mapped pins
TEST_F(GpioUartMapTest, FindByGpioTxAllMappedPins) {
    int gpio_pins[] = {14, 0, 4, 8, 12};
    for (int pin : gpio_pins) {
        auto info = findByGpioTx(pin);
        ASSERT_TRUE(info.has_value()) << "GPIO " << pin << " should be mapped";
    }
}

// GPIO-015: findByUartNumber for all mapped UARTs
TEST_F(GpioUartMapTest, FindByUartNumberAllMapped) {
    int uart_numbers[] = {0, 2, 3, 4, 5};
    for (int num : uart_numbers) {
        auto info = findByUartNumber(num);
        ASSERT_TRUE(info.has_value()) << "UART" << num << " should be mapped";
    }
}
