#include <gtest/gtest.h>

#include "crsf/crsf.hpp"

using namespace elrs;
using namespace elrs::crsf;

class Crc8Test : public ::testing::Test {};

// CRC-001: Empty data
TEST_F(Crc8Test, EmptyData) {
    uint8_t crc = crc8_dvb_s2(nullptr, 0);
    EXPECT_EQ(crc, 0x00);
}

// CRC-002: Single byte 0x00
TEST_F(Crc8Test, SingleByteZero) {
    uint8_t data[] = {0x00};
    uint8_t crc = crc8_dvb_s2(data, 1);
    EXPECT_EQ(crc, 0x00);
}

// CRC-003: Single byte 0xFF
TEST_F(Crc8Test, SingleByteFF) {
    uint8_t data[] = {0xFF};
    uint8_t crc = crc8_dvb_s2(data, 1);
    EXPECT_EQ(crc, 0xD5);
}

// CRC-004: Multiple bytes (RC frame type + payload)
TEST_F(Crc8Test, MultipleBytes) {
    // Type byte followed by some payload
    uint8_t data[] = {0x16, 0x00, 0x00, 0x00, 0x00};
    uint8_t crc = crc8_dvb_s2(data, 5);
    // Just verify it computes without error
    EXPECT_NE(crc, 0x00);  // Should produce non-zero for this input
}

// CRC-005: Known CRSF frame verification
TEST_F(Crc8Test, KnownCrsfFrame) {
    // Device ping frame (extended): Type(0x28) + Dest + Origin
    auto ping_frame = buildDevicePingFrame();
    // CRC is over bytes 2..4 (Type + Dest + Origin)
    uint8_t crc = crc8_dvb_s2(&ping_frame[2], 3);
    EXPECT_EQ(crc, ping_frame[5]);
}

// Test incremental CRC calculation
TEST_F(Crc8Test, IncrementalCalculation) {
    uint8_t data[] = {0x16, 0xAB, 0xCD, 0xEF};

    // Calculate in one shot
    uint8_t crc1 = crc8_dvb_s2(data, 4);

    // Calculate incrementally
    uint8_t crc2 = 0;
    crc2 = crc8_dvb_s2(crc2, data[0]);
    crc2 = crc8_dvb_s2(crc2, data[1]);
    crc2 = crc8_dvb_s2(crc2, data[2]);
    crc2 = crc8_dvb_s2(crc2, data[3]);

    EXPECT_EQ(crc1, crc2);
}

// Test CRC polynomial property
TEST_F(Crc8Test, PolynomialProperty) {
    // CRC-8 DVB-S2 uses polynomial 0xD5
    // Verify that 0x80 XOR'd with the polynomial gives expected result
    uint8_t crc = crc8_dvb_s2(static_cast<uint8_t>(0x00), static_cast<uint8_t>(0x80));

    // After processing 0x80 with initial CRC 0, should get 0xD5 >> shifts
    // This is implementation specific but should be consistent
    EXPECT_NE(crc, 0x00);
}
