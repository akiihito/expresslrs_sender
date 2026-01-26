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

// FRM-003: Device ping frame
TEST_F(CrsfTest, DevicePingFrame) {
    auto frame = buildDevicePingFrame();

    EXPECT_EQ(frame.size(), 4u);
    EXPECT_EQ(frame[0], CRSF_SYNC_BYTE);
    EXPECT_EQ(frame[1], 2);  // Length
    EXPECT_EQ(frame[2], CRSF_FRAME_TYPE_DEVICE_PING);
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
