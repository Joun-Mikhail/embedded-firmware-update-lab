#include "fwupdate/packet.hpp"

#include "byte_io.hpp"
#include "fwupdate/crc32.hpp"

#include <algorithm>

namespace fwupdate {

bool operator==(const Packet& lhs, const Packet& rhs) {
    return lhs.type == rhs.type && lhs.sequence == rhs.sequence && lhs.payload == rhs.payload;
}

bool operator!=(const Packet& lhs, const Packet& rhs) { return !(lhs == rhs); }

std::vector<std::uint8_t> encode_start_payload(const StartInfo& info) {
    std::vector<std::uint8_t> out;
    out.reserve(kStartPayloadLength);
    detail::append_be32(out, info.target_version);
    detail::append_be32(out, info.image_length);
    detail::append_be32(out, info.image_crc);
    return out;
}

std::optional<StartInfo> parse_start_payload(const std::vector<std::uint8_t>& payload) {
    if (payload.size() != kStartPayloadLength) {
        return std::nullopt;
    }
    StartInfo info;
    info.target_version = detail::read_be32(payload, 0);
    info.image_length = detail::read_be32(payload, 4);
    info.image_crc = detail::read_be32(payload, 8);
    return info;
}

Status validate_payload_for_type(PacketType type, std::size_t payload_length) noexcept {
    if (payload_length > kMaxPayloadLength) {
        return Status::PayloadTooLarge;
    }
    switch (type) {
    case PacketType::Start:
        return payload_length == kStartPayloadLength ? Status::Ok : Status::InvalidPayload;
    case PacketType::Data:
        return payload_length >= 1U ? Status::Ok : Status::InvalidPayload;
    case PacketType::Finish:
    case PacketType::Abort:
        return payload_length == 0U ? Status::Ok : Status::InvalidPayload;
    }
    return Status::InvalidPacketType;
}

EncodeResult encode_packet(const Packet& packet) {
    EncodeResult result;
    const auto raw_type = static_cast<std::uint8_t>(packet.type);
    if (!is_valid_packet_type(raw_type)) {
        result.status = Status::InvalidPacketType;
        return result;
    }
    const Status payload_status = validate_payload_for_type(packet.type, packet.payload.size());
    if (payload_status != Status::Ok) {
        result.status = payload_status;
        return result;
    }

    std::vector<std::uint8_t>& out = result.bytes;
    out.reserve(kHeaderLength + packet.payload.size() + kCrcLength);
    out.insert(out.end(), kMagic.begin(), kMagic.end());
    out.push_back(kProtocolVersion);
    out.push_back(raw_type);
    detail::append_be16(out, 0); // reserved
    detail::append_be32(out, packet.sequence);
    // Safe narrowing: payload size was checked against kMaxPayloadLength (256).
    detail::append_be16(out, static_cast<std::uint16_t>(packet.payload.size()));
    out.insert(out.end(), packet.payload.begin(), packet.payload.end());
    const std::uint32_t crc = crc32(out);
    detail::append_be32(out, crc);
    return result;
}

DecodeResult decode_packet(const std::vector<std::uint8_t>& bytes) {
    DecodeResult result;
    auto fail = [&result](Status status) {
        result.status = status;
        result.packet = Packet{};
        return result;
    };

    // 1. Framing: the fixed header and the CRC must be present before any
    //    field is read.
    if (bytes.size() < kMinPacketLength) {
        return fail(Status::InvalidLength);
    }
    // 2. Format identification.
    if (!std::equal(kMagic.begin(), kMagic.end(), bytes.begin())) {
        return fail(Status::InvalidMagic);
    }
    if (bytes[kVersionOffset] != kProtocolVersion) {
        return fail(Status::UnsupportedVersion);
    }
    // 3. Declared length versus the actual buffer. size_t arithmetic cannot
    //    overflow here: declared <= 65535 and the constants are small.
    const std::size_t declared = detail::read_be16(bytes, kLengthOffset);
    if (declared > kMaxPayloadLength) {
        return fail(Status::PayloadTooLarge);
    }
    if (bytes.size() != kHeaderLength + declared + kCrcLength) {
        return fail(Status::InvalidLength); // truncated, or trailing bytes
    }
    // 4. Integrity of everything before the CRC field.
    const std::size_t crc_offset = kHeaderLength + declared;
    Crc32 crc;
    crc.update(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(crc_offset));
    if (crc.value() != detail::read_be32(bytes, crc_offset)) {
        return fail(Status::CrcMismatch);
    }
    // 5. Semantic header fields (only trusted once the CRC has matched).
    const std::uint8_t raw_type = bytes[kTypeOffset];
    if (!is_valid_packet_type(raw_type)) {
        return fail(Status::InvalidPacketType);
    }
    if (detail::read_be16(bytes, kReservedOffset) != 0U) {
        return fail(Status::InvalidReserved);
    }
    const auto type = static_cast<PacketType>(raw_type);
    const Status payload_status = validate_payload_for_type(type, declared);
    if (payload_status != Status::Ok) {
        return fail(payload_status);
    }

    result.status = Status::Ok;
    result.packet.type = type;
    result.packet.sequence = detail::read_be32(bytes, kSequenceOffset);
    result.packet.payload.assign(bytes.begin() + static_cast<std::ptrdiff_t>(kPayloadOffset),
                                 bytes.begin() + static_cast<std::ptrdiff_t>(crc_offset));
    return result;
}

} // namespace fwupdate
