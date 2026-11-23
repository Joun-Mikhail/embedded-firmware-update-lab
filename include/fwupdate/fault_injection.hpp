// Deterministic fault-injection helpers. They operate on encoded packets (or
// sequences of packets) and are deliberately kept outside the parser so that
// normal decoding never depends on them.
//
// Every byte-level mutation returns false and leaves the buffer untouched if
// the requested position does not exist in the buffer.
#pragma once

#include "fwupdate/sender.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace fwupdate::fault {

// ---- Single-packet mutations -------------------------------------------------
bool flip_payload_bit(RawPacket& packet, std::size_t payload_index, unsigned bit);
bool corrupt_crc(RawPacket& packet);                       // inverts the CRC field
bool truncate(RawPacket& packet, std::size_t new_size);    // new_size < size()
bool set_payload_length_field(RawPacket& packet, std::uint16_t value);
bool set_magic(RawPacket& packet, const std::array<std::uint8_t, 4>& magic);
bool set_version(RawPacket& packet, std::uint8_t version);
bool set_type(RawPacket& packet, std::uint8_t type);
bool set_reserved(RawPacket& packet, std::uint16_t value);
bool set_sequence(RawPacket& packet, std::uint32_t sequence);
void append_bytes(RawPacket& packet, std::size_t count, std::uint8_t value);

// Recomputes the trailing CRC over the current contents so that a header or
// payload mutation reaches the checks *behind* the packet CRC.
bool reseal_crc(RawPacket& packet);

// Builds a frame with arbitrary header values and a valid CRC, bypassing the
// encoder's validation (used to craft protocol-violating packets).
RawPacket raw_frame(std::uint8_t type, std::uint32_t sequence,
                    const std::vector<std::uint8_t>& payload,
                    std::uint8_t version = kProtocolVersion,
                    std::uint16_t reserved = 0);

// ---- Stream mutations ----------------------------------------------------------
bool drop_packet(PacketStream& stream, std::size_t index);
bool duplicate_packet(PacketStream& stream, std::size_t index); // inserts copy after index
bool swap_packets(PacketStream& stream, std::size_t first, std::size_t second);

} // namespace fwupdate::fault
