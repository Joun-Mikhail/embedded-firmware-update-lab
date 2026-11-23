// Fault-injection scenarios. Each test documents:
//   original behaviour -> mutation -> expected rejection -> resulting state ->
//   installed-image preservation.
#include "fwupdate/crc32.hpp"
#include "fwupdate/packet.hpp"
#include "helpers.hpp"
#include "testkit.hpp"

#include <cstdio>
#include <functional>
#include <string>

using namespace fwupdate;
using helpers::feed;

// Stream layout with chunk 256 and a 1500-byte image:
//   [0] START  [1..6] DATA seq 1..6 (5x256 + 220)  [7] FINISH seq 7

FW_TEST(FW_003_out_of_order_data, "Swapping two DATA packets is rejected as INVALID_SEQUENCE") {
    auto rx = helpers::make_device();
    PacketStream s = helpers::v2_stream();
    REQUIRE(fault::swap_packets(s, 2, 3));
    const auto r = feed(rx, s);
    CHECK_EQ(r.status, Status::InvalidSequence);
    CHECK_EQ(r.index, std::size_t{2});
    CHECK_EQ(rx.state(), UpdateState::Failed);
    CHECK_EQ(rx.staged_size(), std::size_t{0});
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FW_004_duplicate_data, "A repeated DATA packet is rejected as INVALID_SEQUENCE") {
    auto rx = helpers::make_device();
    PacketStream s = helpers::v2_stream();
    REQUIRE(fault::duplicate_packet(s, 1));
    const auto r = feed(rx, s);
    CHECK_EQ(r.status, Status::InvalidSequence);
    CHECK_EQ(r.index, std::size_t{2});
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FW_005_missing_data, "A dropped DATA packet is detected (sequence gap or incomplete image)") {
    {   // Gap in the middle: the next DATA has the wrong sequence number.
        auto rx = helpers::make_device();
        PacketStream s = helpers::v2_stream();
        REQUIRE(fault::drop_packet(s, 3));
        const auto r = feed(rx, s);
        CHECK_EQ(r.status, Status::InvalidSequence);
        CHECK_EQ(r.index, std::size_t{3});
        CHECK_INSTALLED_IS_V1(rx);
    }
    {   // Last DATA lost and FINISH renumbered to the "expected" sequence:
        // only the image-length check can catch it.
        auto rx = helpers::make_device();
        PacketStream s = helpers::v2_stream();
        REQUIRE(fault::drop_packet(s, 6));
        REQUIRE(fault::set_sequence(s.back(), 6));
        REQUIRE(fault::reseal_crc(s.back()));
        const auto r = feed(rx, s);
        CHECK_EQ(r.status, Status::ImageIncomplete);
        CHECK_EQ(rx.state(), UpdateState::Failed);
        CHECK_INSTALLED_IS_V1(rx);
    }
}

FW_TEST(FW_006_image_corruption_with_valid_packet_crc,
        "Corrupted image bytes with re-sealed packet CRCs are caught only by the image CRC") {
    auto rx = helpers::make_device();
    PacketStream s = helpers::v2_stream();
    REQUIRE(fault::flip_payload_bit(s[4], 17, 3));
    REQUIRE(fault::reseal_crc(s[4]));
    // Every individual packet is now valid on the wire ...
    for (const RawPacket& p : s) {
        REQUIRE_EQ(decode_packet(p).status, Status::Ok);
    }
    // ... so the per-packet checks all pass and only FINISH fails.
    const auto r = feed(rx, s);
    CHECK_EQ(r.status, Status::ImageCrcMismatch);
    CHECK_EQ(r.index, s.size() - 1U);
    CHECK_EQ(rx.state(), UpdateState::Failed);
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FW_010_failed_update_atomicity_matrix,
        "Installed bytes and version are unchanged after every class of failed update") {
    struct Scenario {
        const char* name;
        std::function<void(PacketStream&)> mutate;
        Status expected;
    };
    const Scenario scenarios[] = {
        {"payload bit flip", [](PacketStream& s) { fault::flip_payload_bit(s[2], 0, 0); }, Status::CrcMismatch},
        {"packet CRC corrupted", [](PacketStream& s) { fault::corrupt_crc(s[5]); }, Status::CrcMismatch},
        {"invalid sequence", [](PacketStream& s) { fault::set_sequence(s[3], 42); fault::reseal_crc(s[3]); }, Status::InvalidSequence},
        {"truncated DATA", [](PacketStream& s) { fault::truncate(s[2], 100); }, Status::InvalidLength},
        {"truncated stream (no FINISH)", [](PacketStream& s) { s.pop_back(); }, Status::Ok},
        {"wrong magic", [](PacketStream& s) { fault::set_magic(s[1], {{'B', 'O', 'O', 'T'}}); }, Status::InvalidMagic},
        {"wrong protocol version", [](PacketStream& s) { fault::set_version(s[1], 9); fault::reseal_crc(s[1]); }, Status::UnsupportedVersion},
        {"DATA retyped as START", [](PacketStream& s) { fault::set_type(s[2], 0x01); fault::reseal_crc(s[2]); }, Status::InvalidPayload},
        {"DATA retyped as FINISH", [](PacketStream& s) { fault::set_type(s[2], 0x03); fault::reseal_crc(s[2]); }, Status::InvalidPayload},
        {"length field altered", [](PacketStream& s) { fault::set_payload_length_field(s[1], 255); }, Status::InvalidLength},
        {"image bytes corrupted, CRC resealed", [](PacketStream& s) { fault::flip_payload_bit(s[6], 0, 7); fault::reseal_crc(s[6]); }, Status::ImageCrcMismatch},
        {"version rollback", [](PacketStream& s) { s = *build_update(0, helpers::v2_image()); }, Status::VersionRollback},
        {"aborted update", [](PacketStream& s) { s.resize(3); s.push_back(build_abort(3)); }, Status::UpdateAborted},
    };
    for (const Scenario& sc : scenarios) {
        auto rx = helpers::make_device();
        PacketStream s = helpers::v2_stream();
        sc.mutate(s);
        const auto r = feed(rx, s);
        bool ok = CHECK_EQ(r.status, sc.expected);
        // "truncated stream" never fails a packet: the update simply never
        // completes. It must still not install anything.
        if (sc.expected == Status::Ok) {
            ok = CHECK_EQ(rx.state(), UpdateState::Receiving) && ok;
        }
        ok = CHECK_EQ(rx.installed_version(), helpers::kV1) && ok;
        ok = CHECK(rx.installed_image() == helpers::v1_image()) && ok;
        if (!ok) {
            std::printf("      (scenario: %s)\n", sc.name);
        }
    }
}

FW_TEST(FW_018_data_exceeds_declared_length, "Extra DATA beyond the declared image length is IMAGE_TOO_LARGE") {
    auto rx = helpers::make_device();
    PacketStream s = helpers::v2_stream();
    // Insert an additional DATA (seq 7) before FINISH, renumber FINISH to 8.
    RawPacket extra = encode_packet(Packet{PacketType::Data, 7, {0x01, 0x02}}).bytes;
    s.insert(s.end() - 1, extra);
    REQUIRE(fault::set_sequence(s.back(), 8));
    REQUIRE(fault::reseal_crc(s.back()));
    const auto r = feed(rx, s);
    CHECK_EQ(r.status, Status::ImageTooLarge);
    CHECK_EQ(r.index, std::size_t{7});
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FLT_001_mutators_respect_bounds, "Fault helpers refuse out-of-range positions and leave the buffer unchanged") {
    const RawPacket original = helpers::v2_stream()[7]; // FINISH: 18 bytes, no payload
    RawPacket p = original;
    CHECK(!fault::flip_payload_bit(p, 0, 0)); // no payload to flip
    CHECK(!fault::flip_payload_bit(p, 0, 8));
    CHECK(!fault::truncate(p, p.size()));
    RawPacket tiny{0x46, 0x57};
    CHECK(!fault::corrupt_crc(tiny));
    CHECK(!fault::set_sequence(tiny, 1));
    CHECK(!fault::set_payload_length_field(tiny, 1));
    PacketStream s = helpers::v2_stream();
    CHECK(!fault::drop_packet(s, s.size()));
    CHECK(!fault::swap_packets(s, 1, 1));
    CHECK(!fault::duplicate_packet(s, 99));
    CHECK(p == original);
}
