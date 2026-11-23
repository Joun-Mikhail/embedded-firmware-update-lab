// Packet model plus the encoder and decoder for the FWUP v1 wire format.
#pragma once

#include "fwupdate/protocol.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace fwupdate {

struct Packet {
    PacketType type = PacketType::Data;
    std::uint32_t sequence = 0;
    std::vector<std::uint8_t> payload;
};

bool operator==(const Packet& lhs, const Packet& rhs);
bool operator!=(const Packet& lhs, const Packet& rhs);

// Decoded contents of a START payload.
struct StartInfo {
    std::uint32_t target_version = 0;
    std::uint32_t image_length = 0;
    std::uint32_t image_crc = 0;
};

std::vector<std::uint8_t> encode_start_payload(const StartInfo& info);
// Returns nullopt unless the payload is exactly kStartPayloadLength bytes.
std::optional<StartInfo> parse_start_payload(const std::vector<std::uint8_t>& payload);

// Per-type payload rules: START = 12 bytes, DATA = 1..256 bytes,
// FINISH = 0 bytes, ABORT = 0 bytes.
Status validate_payload_for_type(PacketType type, std::size_t payload_length) noexcept;

struct EncodeResult {
    Status status = Status::Ok;
    std::vector<std::uint8_t> bytes; // empty unless status == Ok
};

struct DecodeResult {
    Status status = Status::Ok;
    Packet packet; // meaningful only if status == Ok
};

// Serialises a packet. Rejects undefined types and illegal payload sizes, so
// every successfully encoded packet is accepted by decode_packet().
EncodeResult encode_packet(const Packet& packet);

// Parses exactly one packet from an untrusted buffer. The buffer must contain
// the packet and nothing else (trailing bytes are rejected).
DecodeResult decode_packet(const std::vector<std::uint8_t>& bytes);

} // namespace fwupdate
