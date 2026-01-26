#include "crsf.hpp"

#include <algorithm>
#include <cstring>

namespace elrs {
namespace crsf {

int16_t pwmToCrsf(int16_t pwm) {
    // Clamp PWM to valid range
    pwm = std::clamp(pwm, PWM_MIN, PWM_MAX);
    // Map 988-2012 us to 172-1811
    return static_cast<int16_t>(
        (static_cast<int32_t>(pwm - PWM_MIN) * (CRSF_CHANNEL_MAX - CRSF_CHANNEL_MIN)) /
        (PWM_MAX - PWM_MIN) + CRSF_CHANNEL_MIN
    );
}

int16_t crsfToPwm(int16_t crsf) {
    // Clamp CRSF to valid range
    crsf = std::clamp(crsf, CRSF_CHANNEL_MIN, CRSF_CHANNEL_MAX);
    // Map 172-1811 to 988-2012 us
    return static_cast<int16_t>(
        (static_cast<int32_t>(crsf - CRSF_CHANNEL_MIN) * (PWM_MAX - PWM_MIN)) /
        (CRSF_CHANNEL_MAX - CRSF_CHANNEL_MIN) + PWM_MIN
    );
}

int16_t clampChannel(int16_t value) {
    return std::clamp(value, CRSF_CHANNEL_MIN, CRSF_CHANNEL_MAX);
}

void packChannels(const ChannelData& channels, uint8_t* output) {
    // Pack 16 x 11-bit channels into 22 bytes
    // Each channel is 11 bits, packed in little-endian order

    uint32_t bits = 0;
    int bit_count = 0;
    int out_idx = 0;

    for (size_t i = 0; i < CRSF_MAX_CHANNELS; i++) {
        int16_t ch = clampChannel(channels[i]);
        bits |= (static_cast<uint32_t>(ch) << bit_count);
        bit_count += CRSF_CHANNEL_BITS;

        while (bit_count >= 8) {
            output[out_idx++] = static_cast<uint8_t>(bits & 0xFF);
            bits >>= 8;
            bit_count -= 8;
        }
    }

    // Handle remaining bits
    if (bit_count > 0) {
        output[out_idx] = static_cast<uint8_t>(bits & 0xFF);
    }
}

void unpackChannels(const uint8_t* input, ChannelData& channels) {
    // Unpack 22 bytes into 16 x 11-bit channels

    uint32_t bits = 0;
    int bit_count = 0;
    int in_idx = 0;

    for (size_t i = 0; i < CRSF_MAX_CHANNELS; i++) {
        while (bit_count < CRSF_CHANNEL_BITS) {
            bits |= (static_cast<uint32_t>(input[in_idx++]) << bit_count);
            bit_count += 8;
        }

        channels[i] = static_cast<int16_t>(bits & 0x7FF);  // 11 bits
        bits >>= CRSF_CHANNEL_BITS;
        bit_count -= CRSF_CHANNEL_BITS;
    }
}

std::array<uint8_t, CRSF_RC_FRAME_SIZE> buildRcChannelsFrame(const ChannelData& channels) {
    std::array<uint8_t, CRSF_RC_FRAME_SIZE> frame{};

    // Sync byte (device address)
    frame[0] = CRSF_SYNC_BYTE;

    // Length: Type (1) + Payload (22) + CRC (1) = 24
    frame[1] = 24;

    // Frame type
    frame[2] = CRSF_FRAME_TYPE_RC_CHANNELS;

    // Pack channels into payload (bytes 3-24)
    packChannels(channels, &frame[3]);

    // Calculate CRC over Type + Payload (bytes 2-24)
    frame[25] = crc8_dvb_s2(&frame[2], 23);

    return frame;
}

std::vector<uint8_t> buildDevicePingFrame() {
    std::vector<uint8_t> frame(4);

    // Sync byte
    frame[0] = CRSF_SYNC_BYTE;

    // Length: Type (1) + CRC (1) = 2
    frame[1] = 2;

    // Frame type
    frame[2] = CRSF_FRAME_TYPE_DEVICE_PING;

    // CRC over Type only
    frame[3] = crc8_dvb_s2(&frame[2], 1);

    return frame;
}

bool validateFrame(const uint8_t* data, size_t len) {
    if (len < 4) {
        return false;
    }

    // Check sync byte
    if (data[0] != CRSF_SYNC_BYTE) {
        return false;
    }

    // Check length field
    uint8_t frame_len = data[1];
    if (len < static_cast<size_t>(frame_len + 2)) {
        return false;
    }

    // Verify CRC (over Type + Payload)
    uint8_t expected_crc = crc8_dvb_s2(&data[2], frame_len - 1);
    uint8_t actual_crc = data[frame_len + 1];

    return expected_crc == actual_crc;
}

uint8_t getFrameType(const uint8_t* data, size_t len) {
    if (len < 3) {
        return 0;
    }
    return data[2];
}

}  // namespace crsf
}  // namespace elrs
