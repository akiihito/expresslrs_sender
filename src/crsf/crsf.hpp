#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "expresslrs_sender/types.hpp"

namespace elrs {
namespace crsf {

// CRC-8 DVB-S2 calculation
uint8_t crc8_dvb_s2(uint8_t crc, uint8_t data);
uint8_t crc8_dvb_s2(const uint8_t* data, size_t len);

// Channel value conversion
int16_t pwmToCrsf(int16_t pwm);
int16_t crsfToPwm(int16_t crsf);
int16_t clampChannel(int16_t value);

// Pack 16 channels (11-bit each) into 22 bytes
void packChannels(const ChannelData& channels, uint8_t* output);

// Unpack 22 bytes into 16 channels
void unpackChannels(const uint8_t* input, ChannelData& channels);

// Build CRSF RC channels frame (26 bytes total)
std::array<uint8_t, CRSF_RC_FRAME_SIZE> buildRcChannelsFrame(const ChannelData& channels);

// Build CRSF device ping frame (extended format with dest/origin addresses)
std::vector<uint8_t> buildDevicePingFrame(uint8_t dest_addr = CRSF_ADDRESS_BROADCAST,
                                          uint8_t origin_addr = CRSF_ADDRESS_HANDSET);

// Extract a complete CRSF frame from a byte buffer
// Returns the number of bytes consumed (0 if no complete frame found)
size_t extractFrame(const uint8_t* data, size_t len, std::vector<uint8_t>& frame_out);

// Parse a DEVICE_INFO response frame
std::optional<DeviceInfo> parseDeviceInfoFrame(const uint8_t* data, size_t len);

// Validate CRSF frame
bool validateFrame(const uint8_t* data, size_t len);

// Get frame type from raw data
uint8_t getFrameType(const uint8_t* data, size_t len);

}  // namespace crsf
}  // namespace elrs
