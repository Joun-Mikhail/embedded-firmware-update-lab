// Firmware update sender: turns an image into a START / DATA... / FINISH
// packet sequence that a FirmwareUpdateReceiver accepts.
#pragma once

#include "fwupdate/protocol.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace fwupdate {

using RawPacket = std::vector<std::uint8_t>;
using PacketStream = std::vector<RawPacket>;

// Builds the encoded packets for a complete update. Returns nullopt if the
// image is empty or larger than kMaxImageLength, or if chunk_size is not in
// [1, kMaxPayloadLength].
std::optional<PacketStream> build_update(std::uint32_t target_version,
                                         const std::vector<std::uint8_t>& image,
                                         std::size_t chunk_size = kMaxPayloadLength);

// Encodes a single ABORT packet with the given sequence number.
RawPacket build_abort(std::uint32_t sequence);

// Deterministic pseudo-random image content (xorshift32) for demos and tests.
std::vector<std::uint8_t> generate_image(std::size_t length, std::uint32_t seed);

} // namespace fwupdate
