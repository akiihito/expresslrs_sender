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

std::vector<uint8_t> buildDevicePingFrame(uint8_t dest_addr, uint8_t origin_addr) {
    std::vector<uint8_t> frame(6);

    // Sync byte (destination address for extended format)
    frame[0] = CRSF_SYNC_BYTE;

    // Length: Type (1) + Dest (1) + Origin (1) + CRC (1) = 4
    frame[1] = 4;

    // Frame type
    frame[2] = CRSF_FRAME_TYPE_DEVICE_PING;

    // Destination and origin addresses
    frame[3] = dest_addr;
    frame[4] = origin_addr;

    // CRC over Type + Dest + Origin (bytes 2..4)
    frame[5] = crc8_dvb_s2(&frame[2], 3);

    return frame;
}

bool validateFrame(const uint8_t* data, size_t len) {
    if (len < 4) {
        return false;
    }

    // Check sync byte (TX module address or FC address)
    if (data[0] != CRSF_SYNC_BYTE && data[0] != CRSF_ADDRESS_FLIGHT_CONTROLLER) {
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

size_t extractFrame(const uint8_t* data, size_t len, std::vector<uint8_t>& frame_out) {
    frame_out.clear();

    // Scan for a valid sync byte
    for (size_t offset = 0; offset < len; offset++) {
        uint8_t sync = data[offset];
        if (sync != CRSF_SYNC_BYTE && sync != CRSF_ADDRESS_FLIGHT_CONTROLLER) {
            continue;
        }

        // Need at least sync + length
        if (offset + 1 >= len) {
            return offset;
        }

        uint8_t frame_len = data[offset + 1];

        // Sanity check: length must be at least 2 (type + crc) and fit in max frame size
        if (frame_len < 2 || frame_len > CRSF_MAX_FRAME_SIZE - 2) {
            continue;
        }

        size_t total_frame_size = static_cast<size_t>(frame_len) + 2;

        // Not enough data yet for the full frame
        if (offset + total_frame_size > len) {
            return offset;
        }

        // Verify CRC
        const uint8_t* frame_start = &data[offset];
        uint8_t expected_crc = crc8_dvb_s2(&frame_start[2], frame_len - 1);
        uint8_t actual_crc = frame_start[frame_len + 1];

        if (expected_crc != actual_crc) {
            continue;
        }

        // Valid frame found
        frame_out.assign(frame_start, frame_start + total_frame_size);
        return offset + total_frame_size;
    }

    // No valid sync byte found, all data consumed
    return len;
}

std::optional<DeviceInfo> parseDeviceInfoFrame(const uint8_t* data, size_t len) {
    // Minimum frame: Sync(1) + Len(1) + Type(1) + Dest(1) + Origin(1) + Name NUL(1)
    //                + Serial(4) + HW(4) + FW(4) + ParamCount(1) + ParamVer(1) + CRC(1) = 21
    if (len < 21) {
        return std::nullopt;
    }

    // Validate as a CRSF frame
    if (!validateFrame(data, len)) {
        return std::nullopt;
    }

    // Check frame type
    if (data[2] != CRSF_FRAME_TYPE_DEVICE_INFO) {
        return std::nullopt;
    }

    // Payload starts after Sync(1) + Len(1) + Type(1)
    // For extended format: Type(1) + Dest(1) + Origin(1) + payload...
    // data[3] = dest, data[4] = origin, data[5..] = device name (NUL terminated)
    size_t payload_start = 5;  // after type + dest + origin

    uint8_t frame_len = data[1];
    size_t frame_end = static_cast<size_t>(frame_len) + 1;  // last byte before CRC (0-indexed from data[0])

    DeviceInfo info;

    // Find NUL-terminated device name
    size_t name_start = payload_start;
    size_t name_end = name_start;
    while (name_end < frame_end && data[name_end] != 0x00) {
        name_end++;
    }

    // Must find NUL terminator within the frame
    if (name_end >= frame_end) {
        return std::nullopt;
    }

    info.device_name = std::string(reinterpret_cast<const char*>(&data[name_start]),
                                   name_end - name_start);

    // After NUL terminator: serial(4) + hw_id(4) + fw_id(4) + param_count(1) + param_ver(1)
    size_t fields_start = name_end + 1;
    size_t fields_needed = 4 + 4 + 4 + 1 + 1;  // 14 bytes

    if (fields_start + fields_needed > frame_end) {
        return std::nullopt;
    }

    std::memcpy(info.serial_number.data(), &data[fields_start], 4);
    std::memcpy(info.hardware_id.data(), &data[fields_start + 4], 4);
    std::memcpy(info.firmware_id.data(), &data[fields_start + 8], 4);
    info.parameter_count = data[fields_start + 12];
    info.parameter_protocol_version = data[fields_start + 13];

    return info;
}

}  // namespace crsf
}  // namespace elrs
