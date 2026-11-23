// Wire-format constants, packet types and result codes for the FWUP v1 protocol.
// See docs/protocol_specification.md for the normative description.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fwupdate {

// ---- Wire format (all multi-byte integers are big-endian) -------------------
//
//  offset  size  field
//  0       4     magic "FWUP"
//  4       1     protocol version (1)
//  5       1     packet type
//  6       2     reserved, must be 0
//  8       4     sequence number
//  12      2     payload length N
//  14      N     payload (0..256 bytes)
//  14+N    4     CRC-32/ISO-HDLC over bytes [0, 14+N)

inline constexpr std::array<std::uint8_t, 4> kMagic{{'F', 'W', 'U', 'P'}};
inline constexpr std::uint8_t kProtocolVersion = 1;

inline constexpr std::size_t kMagicOffset = 0;
inline constexpr std::size_t kVersionOffset = 4;
inline constexpr std::size_t kTypeOffset = 5;
inline constexpr std::size_t kReservedOffset = 6;
inline constexpr std::size_t kSequenceOffset = 8;
inline constexpr std::size_t kLengthOffset = 12;
inline constexpr std::size_t kPayloadOffset = 14;

inline constexpr std::size_t kHeaderLength = 14;
inline constexpr std::size_t kCrcLength = 4;
inline constexpr std::size_t kMaxPayloadLength = 256;
inline constexpr std::size_t kMinPacketLength = kHeaderLength + kCrcLength;                     // 18
inline constexpr std::size_t kMaxPacketLength = kHeaderLength + kMaxPayloadLength + kCrcLength; // 274

// Simulation limit for a complete firmware image (64 KiB).
inline constexpr std::size_t kMaxImageLength = std::size_t{64} * 1024U;

// START payload: target version (4) + image length (4) + image CRC-32 (4).
inline constexpr std::size_t kStartPayloadLength = 12;

enum class PacketType : std::uint8_t {
    Start = 0x01,
    Data = 0x02,
    Finish = 0x03,
    Abort = 0x04,
};

// Returns true if the raw byte names a defined packet type.
constexpr bool is_valid_packet_type(std::uint8_t raw) noexcept {
    return raw >= 0x01U && raw <= 0x04U;
}

enum class Status : std::uint8_t {
    Ok,
    InvalidMagic,      // first four bytes are not "FWUP"
    UnsupportedVersion,// protocol version byte is not 1
    InvalidPacketType, // type byte is not START/DATA/FINISH/ABORT
    InvalidReserved,   // reserved field is not zero
    InvalidLength,     // buffer size inconsistent with header, or illegal image length
    PayloadTooLarge,   // payload longer than kMaxPayloadLength
    InvalidPayload,    // payload size illegal for the packet type
    CrcMismatch,       // packet CRC-32 does not match
    InvalidSequence,   // sequence number is not the expected one
    InvalidState,      // packet not allowed in the current receiver state
    ImageTooLarge,     // declared image or received data exceeds the limit
    ImageIncomplete,   // FINISH received before all image bytes arrived
    ImageCrcMismatch,  // complete image CRC-32 differs from the declared value
    VersionRollback,   // target version is not newer than the installed version
    UpdateAborted,     // an ABORT packet cancelled the active update
};

const char* to_string(Status status) noexcept;
const char* to_string(PacketType type) noexcept;

} // namespace fwupdate
