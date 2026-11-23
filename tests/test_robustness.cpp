// Bounded, deterministic robustness test of the public decoder and receiver.
// This is NOT a coverage-guided fuzzing campaign: it runs a fixed number of
// generated inputs from a fixed seed so any failure is exactly reproducible.
#include "fwupdate/packet.hpp"
#include "helpers.hpp"
#include "testkit.hpp"

#include <cstdio>
#include <random>

using namespace fwupdate;

namespace {
constexpr std::uint32_t kSeed = 20261123U;
constexpr int kCasesPerFamily = 400;
}

FW_TEST(ROB_001_deterministic_malformed_inputs,
        "1200 seeded malformed inputs: no crash/UB, accepted packets are canonical, installed image intact") {
    std::mt19937 rng(kSeed); // mt19937 output is fully specified by the standard
    auto next_byte = [&rng]() { return static_cast<std::uint8_t>(rng() & 0xFFU); };

    const PacketStream valid = helpers::v2_stream(64);
    auto rx = helpers::make_device();
    int accepted = 0;
    int rejected = 0;

    for (int family = 0; family < 3; ++family) {
        for (int i = 0; i < kCasesPerFamily; ++i) {
            RawPacket input;
            if (family == 0) {
                // Pure random bytes, length 0..300.
                const std::size_t len = rng() % 301U;
                for (std::size_t k = 0; k < len; ++k) {
                    input.push_back(next_byte());
                }
            } else if (family == 1) {
                // Valid packet with 1..4 random byte overwrites.
                input = valid[rng() % valid.size()];
                const unsigned edits = 1U + rng() % 4U;
                for (unsigned e = 0; e < edits; ++e) {
                    input[rng() % input.size()] = next_byte();
                }
                // Half of these get a fresh CRC so the mutation reaches the
                // semantic checks (type, reserved, payload rules, sequence).
                if (i % 2 == 0) {
                    fault::reseal_crc(input);
                }
            } else {
                // Valid header prefix, random length cut / extension.
                input = valid[rng() % valid.size()];
                const std::size_t new_len = rng() % (kMaxPacketLength + 8U);
                input.resize(new_len, next_byte());
            }

            const DecodeResult d = decode_packet(input);
            if (d.status == Status::Ok) {
                ++accepted;
                // Property: anything accepted re-encodes to exactly the same bytes.
                const EncodeResult e = encode_packet(d.packet);
                if (!CHECK_EQ(e.status, Status::Ok) || !CHECK(e.bytes == input)) {
                    std::printf("      (family %d case %d)\n", family, i);
                }
            } else {
                ++rejected;
            }
            // Feed the same bytes to a receiver; reset whenever it ends a session.
            rx.receive(input);
            if (rx.state() == UpdateState::Failed || rx.state() == UpdateState::Verified) {
                rx.reset();
            }
            if (rx.installed_version() != helpers::kV1) {
                CHECK(false);
                std::printf("      installed firmware changed (family %d case %d)\n", family, i);
                return;
            }
        }
    }
    std::printf("      cases=%d accepted=%d rejected=%d seed=%u\n", 3 * kCasesPerFamily, accepted,
                rejected, kSeed);
    CHECK_EQ(accepted + rejected, 3 * kCasesPerFamily);
    CHECK(rx.installed_image() == helpers::v1_image());
}
