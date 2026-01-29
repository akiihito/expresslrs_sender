#include <gtest/gtest.h>

#include "crsf/crsf.hpp"

using namespace elrs;
using namespace elrs::crsf;

class CrsfTest : public ::testing::Test {
protected:
    ChannelData centerChannels() {
        ChannelData ch;
        ch.fill(CRSF_CHANNEL_MID);
        return ch;
    }

    ChannelData minChannels() {
        ChannelData ch;
        ch.fill(CRSF_CHANNEL_MIN);
        return ch;
    }

    ChannelData maxChannels() {
        ChannelData ch;
        ch.fill(CRSF_CHANNEL_MAX);
        return ch;
    }
};

// CH-001: PWM to CRSF minimum
TEST_F(CrsfTest, PwmToCrsfMin) {
    EXPECT_EQ(pwmToCrsf(PWM_MIN), CRSF_CHANNEL_MIN);
}

// CH-002: PWM to CRSF center
TEST_F(CrsfTest, PwmToCrsfCenter) {
    EXPECT_EQ(pwmToCrsf(PWM_MID), CRSF_CHANNEL_MID);
}

// CH-003: PWM to CRSF maximum
TEST_F(CrsfTest, PwmToCrsfMax) {
    EXPECT_EQ(pwmToCrsf(PWM_MAX), CRSF_CHANNEL_MAX);
}

// CH-004: CRSF to PWM reverse
TEST_F(CrsfTest, CrsfToPwm) {
    EXPECT_EQ(crsfToPwm(CRSF_CHANNEL_MID), PWM_MID);
}

// CH-005: PWM below range clamps to min
TEST_F(CrsfTest, PwmBelowRangeClamps) {
    EXPECT_EQ(pwmToCrsf(800), CRSF_CHANNEL_MIN);
}

// CH-006: PWM above range clamps to max
TEST_F(CrsfTest, PwmAboveRangeClamps) {
    EXPECT_EQ(pwmToCrsf(2200), CRSF_CHANNEL_MAX);
}

// Channel value clamping
TEST_F(CrsfTest, ClampChannelMin) {
    EXPECT_EQ(clampChannel(0), CRSF_CHANNEL_MIN);
}

TEST_F(CrsfTest, ClampChannelMax) {
    EXPECT_EQ(clampChannel(2000), CRSF_CHANNEL_MAX);
}

TEST_F(CrsfTest, ClampChannelMid) {
    EXPECT_EQ(clampChannel(CRSF_CHANNEL_MID), CRSF_CHANNEL_MID);
}

// FRM-001: RC channels frame with all center values
TEST_F(CrsfTest, RcFrameAllCenter) {
    auto channels = centerChannels();
    auto frame = buildRcChannelsFrame(channels);

    EXPECT_EQ(frame[0], CRSF_SYNC_BYTE);
    EXPECT_EQ(frame[1], 24);  // Length
    EXPECT_EQ(frame[2], CRSF_FRAME_TYPE_RC_CHANNELS);
}

// FRM-002: RC channels frame with min/max values
TEST_F(CrsfTest, RcFrameMinMax) {
    ChannelData channels;
    for (size_t i = 0; i < CRSF_MAX_CHANNELS; i++) {
        channels[i] = (i % 2 == 0) ? CRSF_CHANNEL_MIN : CRSF_CHANNEL_MAX;
    }

    auto frame = buildRcChannelsFrame(channels);

    EXPECT_EQ(frame[0], CRSF_SYNC_BYTE);
    EXPECT_EQ(frame.size(), CRSF_RC_FRAME_SIZE);

    // Verify CRC is valid
    EXPECT_TRUE(validateFrame(frame.data(), frame.size()));
}

// FRM-003: Device ping frame (extended format with dest/origin)
TEST_F(CrsfTest, DevicePingFrame) {
    auto frame = buildDevicePingFrame();

    EXPECT_EQ(frame.size(), 6u);
    EXPECT_EQ(frame[0], CRSF_SYNC_BYTE);
    EXPECT_EQ(frame[1], 4);  // Length: Type + Dest + Origin + CRC
    EXPECT_EQ(frame[2], CRSF_FRAME_TYPE_DEVICE_PING);
    EXPECT_EQ(frame[3], CRSF_ADDRESS_BROADCAST);   // default dest
    EXPECT_EQ(frame[4], CRSF_ADDRESS_HANDSET);      // default origin
    EXPECT_TRUE(validateFrame(frame.data(), frame.size()));
}

// FRM-004: RC frame length
TEST_F(CrsfTest, RcFrameLength) {
    auto channels = centerChannels();
    auto frame = buildRcChannelsFrame(channels);

    EXPECT_EQ(frame.size(), CRSF_RC_FRAME_SIZE);
    EXPECT_EQ(frame.size(), 26u);
}

// FRM-005: Sync byte is always 0xC8
TEST_F(CrsfTest, SyncByte) {
    auto channels = centerChannels();
    auto rc_frame = buildRcChannelsFrame(channels);
    auto ping_frame = buildDevicePingFrame();

    EXPECT_EQ(rc_frame[0], 0xC8);
    EXPECT_EQ(ping_frame[0], 0xC8);
}

// PCK-001: Pack all zeros
TEST_F(CrsfTest, PackAllZeros) {
    ChannelData channels;
    channels.fill(0);  // Will be clamped to min

    uint8_t output[22];
    packChannels(channels, output);

    // All channels at min value should produce known pattern
    // (172 = 0xAC in 11 bits)
    EXPECT_NE(output[0], 0);
}

// PCK-002: Pack all max
TEST_F(CrsfTest, PackAllMax) {
    auto channels = maxChannels();

    uint8_t output[22];
    packChannels(channels, output);

    // Verify packed data is non-zero
    bool all_ff = true;
    for (int i = 0; i < 22; i++) {
        if (output[i] != 0xFF) {
            all_ff = false;
            break;
        }
    }
    // 1811 (max) in 11 bits = 0x713, not 0x7FF
    EXPECT_FALSE(all_ff);
}

// PCK-003: Unpack
TEST_F(CrsfTest, Unpack) {
    auto channels = centerChannels();

    uint8_t packed[22];
    packChannels(channels, packed);

    ChannelData unpacked;
    unpackChannels(packed, unpacked);

    for (size_t i = 0; i < CRSF_MAX_CHANNELS; i++) {
        EXPECT_EQ(unpacked[i], CRSF_CHANNEL_MID);
    }
}

// PCK-004: Round trip
TEST_F(CrsfTest, PackUnpackRoundTrip) {
    ChannelData original;
    for (size_t i = 0; i < CRSF_MAX_CHANNELS; i++) {
        original[i] = static_cast<int16_t>(CRSF_CHANNEL_MIN + (i * 100));
    }

    uint8_t packed[22];
    packChannels(original, packed);

    ChannelData unpacked;
    unpackChannels(packed, unpacked);

    for (size_t i = 0; i < CRSF_MAX_CHANNELS; i++) {
        // Values should match after clamping
        int16_t expected = clampChannel(original[i]);
        EXPECT_EQ(unpacked[i], expected);
    }
}

// Frame validation tests
TEST_F(CrsfTest, ValidateValidFrame) {
    auto channels = centerChannels();
    auto frame = buildRcChannelsFrame(channels);

    EXPECT_TRUE(validateFrame(frame.data(), frame.size()));
}

TEST_F(CrsfTest, ValidateCorruptedCrc) {
    auto channels = centerChannels();
    auto frame = buildRcChannelsFrame(channels);

    // Corrupt CRC
    frame[25] ^= 0xFF;

    EXPECT_FALSE(validateFrame(frame.data(), frame.size()));
}

TEST_F(CrsfTest, ValidateTooShort) {
    uint8_t data[] = {0xC8, 0x02};
    EXPECT_FALSE(validateFrame(data, 2));
}

TEST_F(CrsfTest, ValidateWrongSync) {
    auto channels = centerChannels();
    auto frame = buildRcChannelsFrame(channels);

    frame[0] = 0x00;  // Wrong sync

    EXPECT_FALSE(validateFrame(frame.data(), frame.size()));
}

// Get frame type
TEST_F(CrsfTest, GetFrameType) {
    auto channels = centerChannels();
    auto rc_frame = buildRcChannelsFrame(channels);
    auto ping_frame = buildDevicePingFrame();

    EXPECT_EQ(getFrameType(rc_frame.data(), rc_frame.size()), CRSF_FRAME_TYPE_RC_CHANNELS);
    EXPECT_EQ(getFrameType(ping_frame.data(), ping_frame.size()), CRSF_FRAME_TYPE_DEVICE_PING);
}

// --- Extended Ping Frame Tests ---

TEST_F(CrsfTest, DevicePingFrameCustomAddresses) {
    auto frame = buildDevicePingFrame(CRSF_ADDRESS_TRANSMITTER, CRSF_ADDRESS_FLIGHT_CONTROLLER);

    EXPECT_EQ(frame.size(), 6u);
    EXPECT_EQ(frame[0], CRSF_SYNC_BYTE);
    EXPECT_EQ(frame[1], 4);
    EXPECT_EQ(frame[2], CRSF_FRAME_TYPE_DEVICE_PING);
    EXPECT_EQ(frame[3], CRSF_ADDRESS_TRANSMITTER);
    EXPECT_EQ(frame[4], CRSF_ADDRESS_FLIGHT_CONTROLLER);
    EXPECT_TRUE(validateFrame(frame.data(), frame.size()));
}

TEST_F(CrsfTest, DevicePingFrameCrcValid) {
    auto frame = buildDevicePingFrame();
    // CRC should be over bytes 2..4 (Type + Dest + Origin)
    uint8_t expected_crc = crc8_dvb_s2(&frame[2], 3);
    EXPECT_EQ(frame[5], expected_crc);
}

// --- extractFrame Tests ---

TEST_F(CrsfTest, ExtractFrameValidPing) {
    auto ping = buildDevicePingFrame();
    std::vector<uint8_t> frame_out;

    size_t consumed = extractFrame(ping.data(), ping.size(), frame_out);

    EXPECT_EQ(consumed, ping.size());
    EXPECT_EQ(frame_out, ping);
}

TEST_F(CrsfTest, ExtractFrameWithLeadingGarbage) {
    auto ping = buildDevicePingFrame();
    // Prepend garbage bytes
    std::vector<uint8_t> data = {0x00, 0xFF, 0x42};
    data.insert(data.end(), ping.begin(), ping.end());

    std::vector<uint8_t> frame_out;
    size_t consumed = extractFrame(data.data(), data.size(), frame_out);

    EXPECT_EQ(frame_out, ping);
    EXPECT_EQ(consumed, data.size());  // garbage + frame consumed
}

TEST_F(CrsfTest, ExtractFrameIncomplete) {
    auto ping = buildDevicePingFrame();
    // Only provide first 3 bytes (missing CRC and payload)
    std::vector<uint8_t> partial(ping.begin(), ping.begin() + 3);

    std::vector<uint8_t> frame_out;
    size_t consumed = extractFrame(partial.data(), partial.size(), frame_out);

    EXPECT_TRUE(frame_out.empty());
    // Should stop at sync byte position since frame is incomplete
    EXPECT_EQ(consumed, 0u);
}

TEST_F(CrsfTest, ExtractFrameRcChannels) {
    auto channels = centerChannels();
    auto rc_frame = buildRcChannelsFrame(channels);
    std::vector<uint8_t> data(rc_frame.begin(), rc_frame.end());

    std::vector<uint8_t> frame_out;
    size_t consumed = extractFrame(data.data(), data.size(), frame_out);

    EXPECT_EQ(consumed, rc_frame.size());
    EXPECT_EQ(frame_out.size(), rc_frame.size());
}

TEST_F(CrsfTest, ExtractFrameEmptyBuffer) {
    std::vector<uint8_t> frame_out;
    size_t consumed = extractFrame(nullptr, 0, frame_out);

    EXPECT_EQ(consumed, 0u);
    EXPECT_TRUE(frame_out.empty());
}

// --- parseDeviceInfoFrame Tests ---

// Helper to build a valid DEVICE_INFO frame for testing
static std::vector<uint8_t> buildTestDeviceInfoFrame(const std::string& name) {
    // Frame structure:
    // [0] Sync/Addr  [1] Len  [2] Type  [3] Dest  [4] Origin
    // [5..] DeviceName\0  Serial(4)  HW_ID(4)  FW_ID(4)  ParamCount(1)  ParamVer(1)
    // [last] CRC

    std::vector<uint8_t> frame;
    frame.push_back(CRSF_ADDRESS_FLIGHT_CONTROLLER);  // Sync (response comes from TX module to FC addr)

    // Placeholder for length
    frame.push_back(0);

    frame.push_back(CRSF_FRAME_TYPE_DEVICE_INFO);  // Type
    frame.push_back(CRSF_ADDRESS_HANDSET);           // Dest
    frame.push_back(CRSF_ADDRESS_BROADCAST);         // Origin

    // Device name (NUL-terminated)
    for (char c : name) {
        frame.push_back(static_cast<uint8_t>(c));
    }
    frame.push_back(0x00);  // NUL terminator

    // Serial number (4 bytes)
    frame.push_back(0x01); frame.push_back(0x02);
    frame.push_back(0x03); frame.push_back(0x04);

    // Hardware ID (4 bytes)
    frame.push_back(0xA1); frame.push_back(0xA2);
    frame.push_back(0xA3); frame.push_back(0xA4);

    // Firmware ID (4 bytes)
    frame.push_back(0xF1); frame.push_back(0xF2);
    frame.push_back(0xF3); frame.push_back(0xF4);

    // Parameter count and protocol version
    frame.push_back(10);   // param count
    frame.push_back(1);    // param protocol version

    // Set length: everything from Type to last payload byte + CRC
    // Length = total_size - 2 (sync + len bytes) + 1 (CRC we'll add)
    // Actually: Length = payload_size + 1 (CRC)
    // payload_size = frame.size() - 2 (excluding sync and len)
    frame[1] = static_cast<uint8_t>(frame.size() - 2 + 1);  // +1 for CRC

    // Calculate CRC over Type + Payload (bytes 2 to end)
    uint8_t crc = crc8_dvb_s2(&frame[2], frame.size() - 2);
    frame.push_back(crc);

    return frame;
}

TEST_F(CrsfTest, ParseDeviceInfoValid) {
    auto frame = buildTestDeviceInfoFrame("ELRS TX");

    auto info = parseDeviceInfoFrame(frame.data(), frame.size());

    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->device_name, "ELRS TX");
    EXPECT_EQ(info->serial_number[0], 0x01);
    EXPECT_EQ(info->serial_number[1], 0x02);
    EXPECT_EQ(info->serial_number[2], 0x03);
    EXPECT_EQ(info->serial_number[3], 0x04);
    EXPECT_EQ(info->hardware_id[0], 0xA1);
    EXPECT_EQ(info->hardware_id[3], 0xA4);
    EXPECT_EQ(info->firmware_id[0], 0xF1);
    EXPECT_EQ(info->firmware_id[3], 0xF4);
    EXPECT_EQ(info->parameter_count, 10);
    EXPECT_EQ(info->parameter_protocol_version, 1);
}

TEST_F(CrsfTest, ParseDeviceInfoEmptyName) {
    auto frame = buildTestDeviceInfoFrame("");

    auto info = parseDeviceInfoFrame(frame.data(), frame.size());

    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->device_name, "");
    EXPECT_EQ(info->parameter_count, 10);
}

TEST_F(CrsfTest, ParseDeviceInfoTooShort) {
    // Frame too short to contain all required fields
    uint8_t short_frame[] = {0xC8, 0x03, 0x29, 0x00, 0x00};
    auto info = parseDeviceInfoFrame(short_frame, sizeof(short_frame));

    EXPECT_FALSE(info.has_value());
}

TEST_F(CrsfTest, ParseDeviceInfoWrongType) {
    auto frame = buildTestDeviceInfoFrame("Test");
    // Change type to something else
    frame[2] = CRSF_FRAME_TYPE_DEVICE_PING;
    // Recalculate CRC
    frame.back() = crc8_dvb_s2(&frame[2], frame.size() - 3);

    auto info = parseDeviceInfoFrame(frame.data(), frame.size());

    EXPECT_FALSE(info.has_value());
}

TEST_F(CrsfTest, ParseDeviceInfoBadCrc) {
    auto frame = buildTestDeviceInfoFrame("Test");
    // Corrupt CRC
    frame.back() ^= 0xFF;

    auto info = parseDeviceInfoFrame(frame.data(), frame.size());

    EXPECT_FALSE(info.has_value());
}

// --- validateFrame extended sync byte test ---

TEST_F(CrsfTest, ValidateFrameWithFcAddress) {
    // Build a frame using FC address (0xC8) as sync byte
    auto frame = buildTestDeviceInfoFrame("Test");
    EXPECT_EQ(frame[0], CRSF_ADDRESS_FLIGHT_CONTROLLER);
    EXPECT_TRUE(validateFrame(frame.data(), frame.size()));
}
