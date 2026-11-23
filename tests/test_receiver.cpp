#include "fwupdate/crc32.hpp"
#include "fwupdate/packet.hpp"
#include "helpers.hpp"
#include "testkit.hpp"

using namespace fwupdate;
using helpers::feed;

namespace {

RawPacket start_packet(std::uint32_t version, std::uint32_t length, std::uint32_t crc) {
    return encode_packet(Packet{PacketType::Start, 0, encode_start_payload({version, length, crc})}).bytes;
}

RawPacket finish_packet(std::uint32_t sequence) {
    return encode_packet(Packet{PacketType::Finish, sequence, {}}).bytes;
}

} // namespace

FW_TEST(FW_001_valid_update_v1_to_v2, "A valid update installs v2 and ends in VERIFIED") {
    auto rx = helpers::make_device();
    CHECK_EQ(rx.state(), UpdateState::Idle);
    const PacketStream stream = helpers::v2_stream();
    REQUIRE(!stream.empty());
    const auto r = feed(rx, stream);
    CHECK_EQ(r.status, Status::Ok);
    CHECK_EQ(rx.state(), UpdateState::Verified);
    CHECK_EQ(rx.installed_version(), helpers::kV2);
    CHECK(rx.installed_image() == helpers::v2_image());
    CHECK_EQ(rx.staged_size(), std::size_t{0});
}

FW_TEST(FW_002_multiple_data_packets, "An image split into many small ordered chunks installs correctly") {
    auto rx = helpers::make_device();
    const PacketStream stream = helpers::v2_stream(7); // 1500 / 7 -> 215 DATA packets
    REQUIRE_EQ(stream.size(), std::size_t{1 + 215 + 1});
    // Installed image must stay untouched while chunks arrive.
    for (std::size_t i = 0; i + 1 < stream.size(); ++i) {
        REQUIRE_EQ(rx.receive(stream[i]), Status::Ok);
        CHECK_EQ(rx.installed_version(), helpers::kV1);
    }
    CHECK_EQ(rx.state(), UpdateState::Receiving);
    CHECK_EQ(rx.staged_size(), std::size_t{1500});
    CHECK_EQ(rx.receive(stream.back()), Status::Ok);
    CHECK_EQ(rx.installed_version(), helpers::kV2);
    CHECK(rx.installed_image() == helpers::v2_image());
}

FW_TEST(FW_007_version_rollback, "START with an older version is rejected as VERSION_ROLLBACK") {
    auto rx = fwupdate::FirmwareUpdateReceiver(helpers::v2_image(), 5);
    auto stream = build_update(4, helpers::v1_image());
    REQUIRE(stream.has_value());
    const auto r = feed(rx, *stream);
    CHECK_EQ(r.status, Status::VersionRollback);
    CHECK_EQ(r.index, std::size_t{0});
    CHECK_EQ(rx.state(), UpdateState::Failed);
    CHECK_EQ(rx.installed_version(), 5U);
    CHECK(rx.installed_image() == helpers::v2_image());
}

FW_TEST(FW_008_equal_version_rejected, "Reinstalling the installed version is rejected (strictly increasing policy)") {
    auto rx = helpers::make_device();
    auto stream = build_update(helpers::kV1, helpers::v2_image());
    REQUIRE(stream.has_value());
    CHECK_EQ(feed(rx, *stream).status, Status::VersionRollback);
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FW_009_abort_active_update, "ABORT mid-transfer returns to IDLE and keeps v1 installed") {
    auto rx = helpers::make_device();
    const PacketStream stream = helpers::v2_stream(100);
    REQUIRE_EQ(rx.receive(stream[0]), Status::Ok);
    REQUIRE_EQ(rx.receive(stream[1]), Status::Ok);
    REQUIRE_EQ(rx.receive(stream[2]), Status::Ok);
    CHECK_EQ(rx.staged_size(), std::size_t{200});
    CHECK_EQ(rx.receive(build_abort(3)), Status::UpdateAborted);
    CHECK_EQ(rx.state(), UpdateState::Idle);
    CHECK_EQ(rx.staged_size(), std::size_t{0});
    CHECK_INSTALLED_IS_V1(rx);
    // IDLE after abort: a fresh complete update succeeds without reset().
    CHECK_EQ(feed(rx, stream).status, Status::Ok);
    CHECK_EQ(rx.installed_version(), helpers::kV2);
}

FW_TEST(FW_009b_abort_rules, "ABORT is only valid while RECEIVING and with the expected sequence number") {
    auto idle = helpers::make_device();
    CHECK_EQ(idle.receive(build_abort(0)), Status::InvalidState);
    CHECK_EQ(idle.state(), UpdateState::Failed);
    CHECK_INSTALLED_IS_V1(idle);

    auto rx = helpers::make_device();
    const PacketStream stream = helpers::v2_stream(100);
    REQUIRE_EQ(rx.receive(stream[0]), Status::Ok);
    REQUIRE_EQ(rx.receive(stream[1]), Status::Ok);
    CHECK_EQ(rx.receive(build_abort(1)), Status::InvalidSequence); // stale ABORT
    CHECK_EQ(rx.state(), UpdateState::Failed);
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FW_011_recovery_after_failure, "reset() from FAILED allows a subsequent valid update") {
    auto rx = helpers::make_device();
    PacketStream bad = helpers::v2_stream();
    REQUIRE(fault::corrupt_crc(bad[2]));
    CHECK_EQ(feed(rx, bad).status, Status::CrcMismatch);
    CHECK_EQ(rx.state(), UpdateState::Failed);
    // FAILED is sticky: even a valid START is refused until reset().
    CHECK_EQ(rx.receive(helpers::v2_stream()[0]), Status::InvalidState);
    rx.reset();
    CHECK_EQ(rx.state(), UpdateState::Idle);
    CHECK_EQ(feed(rx, helpers::v2_stream()).status, Status::Ok);
    CHECK_EQ(rx.state(), UpdateState::Verified);
    CHECK_EQ(rx.installed_version(), helpers::kV2);
}

FW_TEST(FW_011b_repeated_failures_then_recovery, "Several consecutive failed attempts never alter v1; the next valid one succeeds") {
    auto rx = helpers::make_device();
    for (int attempt = 0; attempt < 5; ++attempt) {
        PacketStream bad = helpers::v2_stream();
        REQUIRE(fault::flip_payload_bit(bad[1], static_cast<std::size_t>(attempt), 0));
        CHECK_EQ(feed(rx, bad).status, Status::CrcMismatch);
        CHECK_INSTALLED_IS_V1(rx);
        rx.reset();
    }
    CHECK_EQ(feed(rx, helpers::v2_stream()).status, Status::Ok);
    CHECK_EQ(rx.installed_version(), helpers::kV2);
}

FW_TEST(FW_012_image_size_boundaries, "64 KiB image installs; 64 KiB + 1 and zero-length are rejected at START") {
    {
        auto rx = helpers::make_device();
        const auto image = generate_image(kMaxImageLength, 0xABCDEF01U);
        auto stream = build_update(2, image);
        REQUIRE(stream.has_value());
        CHECK_EQ(stream->size(), std::size_t{1 + 256 + 1});
        CHECK_EQ(feed(rx, *stream).status, Status::Ok);
        CHECK(rx.installed_image() == image);
    }
    {
        auto rx = helpers::make_device();
        CHECK_EQ(rx.receive(start_packet(2, kMaxImageLength + 1U, 0)), Status::ImageTooLarge);
        CHECK_INSTALLED_IS_V1(rx);
    }
    {
        auto rx = helpers::make_device();
        CHECK_EQ(rx.receive(start_packet(2, 0xFFFFFFFFU, 0)), Status::ImageTooLarge);
        CHECK_INSTALLED_IS_V1(rx);
    }
    {
        auto rx = helpers::make_device();
        CHECK_EQ(rx.receive(start_packet(2, 0, crc32({}))), Status::InvalidLength);
        CHECK_EQ(rx.state(), UpdateState::Failed);
        CHECK_INSTALLED_IS_V1(rx);
    }
    CHECK(!build_update(2, {}).has_value());
    CHECK(!build_update(2, std::vector<std::uint8_t>(kMaxImageLength + 1U, 0)).has_value());
}

FW_TEST(FW_013_data_or_finish_before_start, "DATA or FINISH while IDLE is INVALID_STATE and changes nothing installed") {
    const PacketStream stream = helpers::v2_stream();
    auto rx1 = helpers::make_device();
    CHECK_EQ(rx1.receive(stream[1]), Status::InvalidState);
    CHECK_EQ(rx1.state(), UpdateState::Failed);
    CHECK_INSTALLED_IS_V1(rx1);

    auto rx2 = helpers::make_device();
    CHECK_EQ(rx2.receive(finish_packet(1)), Status::InvalidState);
    CHECK_INSTALLED_IS_V1(rx2);
}

FW_TEST(FW_014_packets_after_completion, "Packets after VERIFIED are rejected until reset(); v2 stays installed") {
    auto rx = helpers::make_device();
    const PacketStream stream = helpers::v2_stream();
    REQUIRE_EQ(feed(rx, stream).status, Status::Ok);
    CHECK_EQ(rx.receive(stream[1]), Status::InvalidState);              // late DATA
    CHECK_EQ(rx.receive(stream.back()), Status::InvalidState);          // repeated FINISH
    CHECK_EQ(rx.state(), UpdateState::Verified);                        // terminal state kept
    CHECK_EQ(rx.installed_version(), helpers::kV2);
    CHECK(rx.installed_image() == helpers::v2_image());
    // Replaying the whole v2 stream after reset() is a rollback-to-equal.
    rx.reset();
    CHECK_EQ(feed(rx, stream).status, Status::VersionRollback);
    CHECK_EQ(rx.installed_version(), helpers::kV2);
}

FW_TEST(FW_015_start_while_receiving, "A second START during an active transfer is INVALID_STATE") {
    auto rx = helpers::make_device();
    const PacketStream stream = helpers::v2_stream();
    REQUIRE_EQ(rx.receive(stream[0]), Status::Ok);
    CHECK_EQ(rx.receive(stream[0]), Status::InvalidState);
    CHECK_EQ(rx.state(), UpdateState::Failed);
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FW_016_start_with_nonzero_sequence, "START must carry sequence number zero") {
    auto rx = helpers::make_device();
    RawPacket start = helpers::v2_stream()[0];
    REQUIRE(fault::set_sequence(start, 1));
    REQUIRE(fault::reseal_crc(start));
    CHECK_EQ(rx.receive(start), Status::InvalidSequence);
    CHECK_INSTALLED_IS_V1(rx);
}

FW_TEST(FW_017_handle_rejects_malformed_packet_objects, "handle() enforces payload rules for packets that bypass the decoder") {
    auto rx = helpers::make_device();
    CHECK_EQ(rx.handle(Packet{PacketType::Data, 1, {}}), Status::InvalidPayload);
    auto rx2 = helpers::make_device();
    CHECK_EQ(rx2.handle(Packet{PacketType::Data, 1, std::vector<std::uint8_t>(300, 0)}),
             Status::PayloadTooLarge);
    CHECK_INSTALLED_IS_V1(rx);
    CHECK_INSTALLED_IS_V1(rx2);

    // An undefined type value cannot come off the wire (decode rejects it),
    // but a caller of handle() can construct one. It must fail closed, both
    // from IDLE and in the middle of an active session.
    auto rx3 = helpers::make_device();
    CHECK_EQ(rx3.handle(Packet{static_cast<PacketType>(0x09), 0, {}}), Status::InvalidPacketType);
    CHECK_EQ(rx3.state(), UpdateState::Failed);
    CHECK_INSTALLED_IS_V1(rx3);

    auto rx4 = helpers::make_device();
    const PacketStream stream = helpers::v2_stream(100);
    REQUIRE_EQ(rx4.receive(stream[0]), Status::Ok);
    REQUIRE_EQ(rx4.receive(stream[1]), Status::Ok);
    CHECK_EQ(rx4.handle(Packet{static_cast<PacketType>(0x00), 2, {0xAA}}), Status::InvalidPacketType);
    CHECK_EQ(rx4.state(), UpdateState::Failed);
    CHECK_EQ(rx4.staged_size(), std::size_t{0});
    CHECK_INSTALLED_IS_V1(rx4);
}
