#include "fwupdate/crc32.hpp"
#include "fwupdate/fault_injection.hpp"
#include "fwupdate/packet.hpp"
#include "testkit.hpp"

#include <cstdio>

using namespace fwupdate;

namespace {

Packet sample_data_packet() {
    Packet p;
    p.type = PacketType::Data;
    p.sequence = 0x01020304U;
    p.payload = {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x7F};
    return p;
}

RawPacket encode_ok(const Packet& p) {
    EncodeResult r = encode_packet(p);
    REQUIRE_EQ(r.status, Status::Ok);
    return r.bytes;
}

} // namespace

FW_TEST(PKT_001_valid_round_trip, "Encode then decode returns identical fields for every packet type") {
    const Packet data = sample_data_packet();
    const RawPacket bytes = encode_ok(data);
    REQUIRE_EQ(bytes.size(), kHeaderLength + data.payload.size() + kCrcLength);

    // Exact wire layout: magic, version, type, reserved, big-endian seq and length.
    const std::vector<std::uint8_t> header(bytes.begin(), bytes.begin() + 14);
    const std::vector<std::uint8_t> expected_header{'F', 'W', 'U', 'P', 0x01, 0x02, 0x00, 0x00,
                                                    0x01, 0x02, 0x03, 0x04, 0x00, 0x06};
    CHECK(header == expected_header);
    const std::vector<std::uint8_t> covered(bytes.begin(), bytes.end() - 4);
    const std::uint32_t crc = crc32(covered);
    CHECK_EQ(bytes[20], static_cast<std::uint8_t>(crc >> 24));
    CHECK_EQ(bytes[23], static_cast<std::uint8_t>(crc & 0xFFU));

    DecodeResult decoded = decode_packet(bytes);
    REQUIRE_EQ(decoded.status, Status::Ok);
    CHECK_EQ(decoded.packet.type, data.type);
    CHECK_EQ(decoded.packet.sequence, data.sequence);
    CHECK(decoded.packet.payload == data.payload);

    const Packet others[] = {
        {PacketType::Start, 0, encode_start_payload(StartInfo{2, 1500, 0xCAFEBABEU})},
        {PacketType::Finish, 7, {}},
        {PacketType::Abort, 3, {}},
        {PacketType::Data, 0xFFFFFFFFU, std::vector<std::uint8_t>(kMaxPayloadLength, 0xA5)},
    };
    for (const Packet& p : others) {
        DecodeResult r = decode_packet(encode_ok(p));
        CHECK_EQ(r.status, Status::Ok);
        CHECK(r.packet == p);
    }
    const auto info = parse_start_payload(others[0].payload);
    REQUIRE(info.has_value());
    CHECK_EQ(info->target_version, 2U);
    CHECK_EQ(info->image_length, 1500U);
    CHECK_EQ(info->image_crc, 0xCAFEBABEU);
}

FW_TEST(PKT_003_invalid_magic, "A packet whose magic is not FWUP is rejected as INVALID_MAGIC") {
    RawPacket bytes = encode_ok(sample_data_packet());
    REQUIRE(fault::set_magic(bytes, {{'F', 'W', 'U', 'X'}}));
    CHECK_EQ(decode_packet(bytes).status, Status::InvalidMagic);
    REQUIRE(fault::reseal_crc(bytes)); // a valid CRC must not rescue it
    CHECK_EQ(decode_packet(bytes).status, Status::InvalidMagic);
}

FW_TEST(PKT_004_unsupported_version, "Protocol versions other than 1 are rejected as UNSUPPORTED_VERSION") {
    for (std::uint8_t v : {std::uint8_t{0}, std::uint8_t{2}, std::uint8_t{0xFF}}) {
        RawPacket bytes = encode_ok(sample_data_packet());
        REQUIRE(fault::set_version(bytes, v));
        REQUIRE(fault::reseal_crc(bytes));
        CHECK_EQ(decode_packet(bytes).status, Status::UnsupportedVersion);
    }
}

FW_TEST(PKT_005_truncated_packet, "Truncation at every boundary (header, payload, CRC) is INVALID_LENGTH") {
    const RawPacket full = encode_ok(sample_data_packet()); // 24 bytes
    for (std::size_t size = 0; size < full.size(); ++size) {
        RawPacket bytes = full;
        REQUIRE(fault::truncate(bytes, size));
        const Status s = decode_packet(bytes).status;
        if (!CHECK_EQ(s, Status::InvalidLength)) {
            std::printf("      (truncated to %zu bytes)\n", size);
        }
    }
}

FW_TEST(PKT_006_incorrect_advertised_length, "Length field inconsistent with buffer size is INVALID_LENGTH") {
    const RawPacket full = encode_ok(sample_data_packet()); // payload length 6
    for (std::uint16_t declared : {std::uint16_t{0}, std::uint16_t{5}, std::uint16_t{7},
                                   std::uint16_t{256}}) {
        RawPacket bytes = full;
        REQUIRE(fault::set_payload_length_field(bytes, declared));
        CHECK_EQ(decode_packet(bytes).status, Status::InvalidLength);
    }
    // Trailing bytes after a well-formed packet are not allowed either.
    RawPacket trailing = full;
    fault::append_bytes(trailing, 1, 0x00);
    CHECK_EQ(decode_packet(trailing).status, Status::InvalidLength);
}

FW_TEST(PKT_007_invalid_packet_type, "Undefined type values are rejected as INVALID_PACKET_TYPE") {
    for (std::uint8_t t : {std::uint8_t{0x00}, std::uint8_t{0x05}, std::uint8_t{0x80},
                           std::uint8_t{0xFF}}) {
        RawPacket bytes = encode_ok(sample_data_packet());
        REQUIRE(fault::set_type(bytes, t));
        REQUIRE(fault::reseal_crc(bytes));
        CHECK_EQ(decode_packet(bytes).status, Status::InvalidPacketType);
    }
    Packet bogus = sample_data_packet();
    bogus.type = static_cast<PacketType>(0x09);
    CHECK_EQ(encode_packet(bogus).status, Status::InvalidPacketType);
}

FW_TEST(PKT_008_oversized_payload, "Payloads above 256 bytes are rejected by encoder and decoder") {
    Packet big{PacketType::Data, 1, std::vector<std::uint8_t>(kMaxPayloadLength + 1U, 0x11)};
    EncodeResult enc = encode_packet(big);
    CHECK_EQ(enc.status, Status::PayloadTooLarge);
    CHECK(enc.bytes.empty());

    // Wire frame claiming and carrying 257 bytes, with a valid CRC.
    const RawPacket wire = fault::raw_frame(0x02, 1, big.payload);
    REQUIRE_EQ(wire.size(), kMaxPacketLength + 1U);
    CHECK_EQ(decode_packet(wire).status, Status::PayloadTooLarge);

    // Exactly 256 bytes is still accepted (upper boundary).
    big.payload.pop_back();
    CHECK_EQ(decode_packet(encode_ok(big)).status, Status::Ok);
}

FW_TEST(PKT_009_corrupted_payload_crc, "Payload bit flips or a damaged CRC field yield CRC_MISMATCH") {
    const RawPacket full = encode_ok(sample_data_packet());
    for (std::size_t i = 0; i < 6; ++i) {
        for (unsigned bit = 0; bit < 8; ++bit) {
            RawPacket bytes = full;
            REQUIRE(fault::flip_payload_bit(bytes, i, bit));
            CHECK_EQ(decode_packet(bytes).status, Status::CrcMismatch);
        }
    }
    RawPacket bad_crc = full;
    REQUIRE(fault::corrupt_crc(bad_crc));
    CHECK_EQ(decode_packet(bad_crc).status, Status::CrcMismatch);
    // A header mutation without resealing is also caught by the CRC.
    RawPacket bad_seq = full;
    REQUIRE(fault::set_sequence(bad_seq, 99));
    CHECK_EQ(decode_packet(bad_seq).status, Status::CrcMismatch);
}

FW_TEST(PKT_010_reserved_field_nonzero, "Nonzero reserved bits are rejected as INVALID_RESERVED") {
    for (std::uint16_t r : {std::uint16_t{0x0001}, std::uint16_t{0x8000}, std::uint16_t{0xFFFF}}) {
        const RawPacket bytes = fault::raw_frame(0x02, 1, {0xAA}, kProtocolVersion, r);
        CHECK_EQ(decode_packet(bytes).status, Status::InvalidReserved);
    }
}

FW_TEST(PKT_011_empty_and_minimal_buffers, "Empty input and per-type payload rules are enforced") {
    CHECK_EQ(decode_packet({}).status, Status::InvalidLength);
    CHECK_EQ(decode_packet(std::vector<std::uint8_t>(kMinPacketLength - 1U, 0)).status,
             Status::InvalidLength);
    // Structurally valid frames whose payload size is illegal for their type.
    CHECK_EQ(decode_packet(fault::raw_frame(0x02, 1, {})).status, Status::InvalidPayload);  // empty DATA
    CHECK_EQ(decode_packet(fault::raw_frame(0x01, 0, {1, 2, 3})).status, Status::InvalidPayload); // short START
    CHECK_EQ(decode_packet(fault::raw_frame(0x03, 1, {0})).status, Status::InvalidPayload); // FINISH w/ data
    CHECK_EQ(decode_packet(fault::raw_frame(0x04, 1, {0})).status, Status::InvalidPayload); // ABORT w/ data
    CHECK_EQ(encode_packet(Packet{PacketType::Data, 1, {}}).status, Status::InvalidPayload);
    // Minimal valid packet: FINISH with no payload is exactly 18 bytes.
    const RawPacket finish = encode_ok(Packet{PacketType::Finish, 1, {}});
    CHECK_EQ(finish.size(), kMinPacketLength);
}

FW_TEST(PKT_012_overflow_sensitive_lengths, "Maximum 16-bit length field cannot cause over-read or overflow") {
    // Header advertises 65535 payload bytes but only 4 bytes follow.
    RawPacket bytes = encode_ok(sample_data_packet());
    REQUIRE(fault::set_payload_length_field(bytes, 0xFFFF));
    CHECK_EQ(decode_packet(bytes).status, Status::PayloadTooLarge);
    // A buffer that really is 65535 + 18 bytes long is rejected before CRC work.
    RawPacket huge = fault::raw_frame(0x02, 1, std::vector<std::uint8_t>(0xFFFF, 0x5A));
    CHECK_EQ(decode_packet(huge).status, Status::PayloadTooLarge);
}
