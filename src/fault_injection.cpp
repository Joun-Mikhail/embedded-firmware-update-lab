#include "fwupdate/fault_injection.hpp"

#include "byte_io.hpp"
#include "fwupdate/crc32.hpp"

#include <utility>

namespace fwupdate::fault {
namespace {

bool has_bytes(const RawPacket& packet, std::size_t offset, std::size_t width) {
    return offset <= packet.size() && width <= packet.size() - offset;
}

} // namespace

bool flip_payload_bit(RawPacket& packet, std::size_t payload_index, unsigned bit) {
    if (bit > 7U || packet.size() < kMinPacketLength) {
        return false;
    }
    const std::size_t payload_length = packet.size() - kMinPacketLength;
    if (payload_index >= payload_length) {
        return false;
    }
    const std::size_t index = kPayloadOffset + payload_index;
    packet[index] = static_cast<std::uint8_t>(packet[index] ^ (1U << bit));
    return true;
}

bool corrupt_crc(RawPacket& packet) {
    if (packet.size() < kMinPacketLength) {
        return false;
    }
    const std::size_t offset = packet.size() - kCrcLength;
    detail::write_be32(packet, offset, ~detail::read_be32(packet, offset));
    return true;
}

bool truncate(RawPacket& packet, std::size_t new_size) {
    if (new_size >= packet.size()) {
        return false;
    }
    packet.resize(new_size);
    return true;
}

bool set_payload_length_field(RawPacket& packet, std::uint16_t value) {
    if (!has_bytes(packet, kLengthOffset, 2)) {
        return false;
    }
    detail::write_be16(packet, kLengthOffset, value);
    return true;
}

bool set_magic(RawPacket& packet, const std::array<std::uint8_t, 4>& magic) {
    if (!has_bytes(packet, kMagicOffset, magic.size())) {
        return false;
    }
    for (std::size_t i = 0; i < magic.size(); ++i) {
        packet[kMagicOffset + i] = magic[i];
    }
    return true;
}

bool set_version(RawPacket& packet, std::uint8_t version) {
    if (!has_bytes(packet, kVersionOffset, 1)) {
        return false;
    }
    packet[kVersionOffset] = version;
    return true;
}

bool set_type(RawPacket& packet, std::uint8_t type) {
    if (!has_bytes(packet, kTypeOffset, 1)) {
        return false;
    }
    packet[kTypeOffset] = type;
    return true;
}

bool set_reserved(RawPacket& packet, std::uint16_t value) {
    if (!has_bytes(packet, kReservedOffset, 2)) {
        return false;
    }
    detail::write_be16(packet, kReservedOffset, value);
    return true;
}

bool set_sequence(RawPacket& packet, std::uint32_t sequence) {
    if (!has_bytes(packet, kSequenceOffset, 4)) {
        return false;
    }
    detail::write_be32(packet, kSequenceOffset, sequence);
    return true;
}

void append_bytes(RawPacket& packet, std::size_t count, std::uint8_t value) {
    packet.insert(packet.end(), count, value);
}

bool reseal_crc(RawPacket& packet) {
    if (packet.size() < kCrcLength) {
        return false;
    }
    const std::size_t crc_offset = packet.size() - kCrcLength;
    Crc32 crc;
    crc.update(packet.begin(), packet.begin() + static_cast<std::ptrdiff_t>(crc_offset));
    detail::write_be32(packet, crc_offset, crc.value());
    return true;
}

RawPacket raw_frame(std::uint8_t type, std::uint32_t sequence,
                    const std::vector<std::uint8_t>& payload, std::uint8_t version,
                    std::uint16_t reserved) {
    RawPacket out;
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    out.push_back(version);
    out.push_back(type);
    detail::append_be16(out, reserved);
    detail::append_be32(out, sequence);
    // The length field is 16 bits wide; callers needing an inconsistent length
    // use set_payload_length_field() afterwards.
    detail::append_be16(out, static_cast<std::uint16_t>(payload.size() & 0xFFFFU));
    out.insert(out.end(), payload.begin(), payload.end());
    detail::append_be32(out, crc32(out));
    return out;
}

bool drop_packet(PacketStream& stream, std::size_t index) {
    if (index >= stream.size()) {
        return false;
    }
    stream.erase(stream.begin() + static_cast<std::ptrdiff_t>(index));
    return true;
}

bool duplicate_packet(PacketStream& stream, std::size_t index) {
    if (index >= stream.size()) {
        return false;
    }
    RawPacket copy = stream[index];
    stream.insert(stream.begin() + static_cast<std::ptrdiff_t>(index + 1U), std::move(copy));
    return true;
}

bool swap_packets(PacketStream& stream, std::size_t first, std::size_t second) {
    if (first >= stream.size() || second >= stream.size() || first == second) {
        return false;
    }
    std::swap(stream[first], stream[second]);
    return true;
}

} // namespace fwupdate::fault
