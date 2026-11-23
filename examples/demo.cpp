// End-to-end demonstration. Every verdict printed here is derived from the
// return values of the real decoder/receiver; nothing is hard-coded.
// Exit code 0 only if every expected behaviour was observed.
//
// Usage: fwupdate_demo [--color]
#include "fwupdate/crc32.hpp"
#include "fwupdate/fault_injection.hpp"
#include "fwupdate/packet.hpp"
#include "fwupdate/receiver.hpp"
#include "fwupdate/sender.hpp"

#include <cstdio>
#include <cstring>
#include <string>

using namespace fwupdate;

namespace {

bool g_color = false;

const char* c(const char* code) { return g_color ? code : ""; }
const char* reset() { return c("\033[0m"); }

void tag(const char* label, const char* color) {
    std::printf("%s[%-6s]%s ", c(color), label, reset());
}

constexpr const char* kBlue = "\033[1;34m";
constexpr const char* kTeal = "\033[36m";
constexpr const char* kGreen = "\033[1;32m";
constexpr const char* kRed = "\033[1;31m";
constexpr const char* kAmber = "\033[33m";

int g_failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        ++g_failures;
        tag("ERROR", kAmber);
        std::printf("unexpected behaviour: %s\n", what);
    }
}

struct TransferStats {
    Status status = Status::Ok;
    std::size_t data_packets = 0;
    std::size_t data_ok = 0;
    std::string failed_type;
};

TransferStats send_stream(FirmwareUpdateReceiver& rx, const PacketStream& stream) {
    TransferStats st;
    for (const RawPacket& raw : stream) {
        const DecodeResult peek = decode_packet(raw);
        const bool is_data = peek.status == Status::Ok && peek.packet.type == PacketType::Data;
        st.data_packets += is_data ? 1U : 0U;
        const Status s = rx.receive(raw);
        if (s != Status::Ok) {
            st.status = s;
            st.failed_type = peek.status == Status::Ok ? to_string(peek.packet.type) : "UNDECODABLE";
            return st;
        }
        st.data_ok += is_data ? 1U : 0U;
    }
    return st;
}

void print_rule() { std::printf("%s----------------------------------------%s\n", c(kTeal), reset()); }

} // namespace

int main(int argc, char** argv) {
    g_color = argc > 1 && std::strcmp(argv[1], "--color") == 0;

    std::printf("%sEmbedded Firmware Update Lab%s\n", c(kBlue), reset());
    print_rule();

    // 1. Device with firmware v1.
    FirmwareUpdateReceiver device(generate_image(4096, 0x0000A001U), 1);
    tag("DEVICE", kBlue);
    std::printf("Installed firmware: v%u (%zu bytes, CRC-32 0x%08X)\n", device.installed_version(),
                device.installed_image().size(), crc32(device.installed_image()));

    // 2-4. Deterministic v2 image and its packet stream.
    const std::vector<std::uint8_t> v2 = generate_image(10000, 0x0000B002U);
    const auto v2_stream = build_update(2, v2);
    expect(v2_stream.has_value(), "v2 update could not be built");
    if (!v2_stream) {
        return 1;
    }
    std::printf("\n");
    tag("START", kTeal);
    std::printf("Update requested: v2 (%zu bytes, image CRC-32 0x%08X, %zu packets)\n", v2.size(),
                crc32(v2), v2_stream->size());

    // 5-7. Send through the real decoder/receiver.
    const TransferStats s2 = send_stream(device, *v2_stream);
    tag("DATA", kTeal);
    std::printf("Received %zu/%zu DATA packets\n", s2.data_ok, s2.data_packets);
    const bool v2_ok = s2.status == Status::Ok && device.state() == UpdateState::Verified;
    tag("CHECK", v2_ok ? kGreen : kRed);
    std::printf("Packet + image integrity: %s%s%s (%s)\n", c(v2_ok ? kGreen : kRed),
                v2_ok ? "PASSED" : "FAILED", reset(), to_string(s2.status));
    expect(v2_ok, "valid v2 update was not accepted");
    expect(device.installed_version() == 2U && device.installed_image() == v2,
           "v2 was not installed");
    tag("COMMIT", v2_ok ? kGreen : kRed);
    std::printf("Installed firmware: v%u, state %s\n", device.installed_version(),
                to_string(device.state()));

    // Policy: VERIFIED requires reset() before the next session.
    device.reset();

    // 8-10. v3 update with image corruption hidden behind valid packet CRCs.
    const std::vector<std::uint8_t> v3 = generate_image(12000, 0x0000C003U);
    auto v3_stream = build_update(3, v3);
    expect(v3_stream.has_value(), "v3 update could not be built");
    if (!v3_stream) {
        return 1;
    }
    std::printf("\n");
    tag("START", kTeal);
    std::printf("Update requested: v3 (%zu bytes, %zu packets)\n", v3.size(), v3_stream->size());
    const std::size_t victim = 20; // a DATA packet in the middle of the image
    const bool injected = fault::flip_payload_bit((*v3_stream)[victim], 100, 2) &&
                          fault::reseal_crc((*v3_stream)[victim]);
    expect(injected, "fault injection failed");
    tag("FAULT", kAmber);
    std::printf("Flipped bit 2 of payload byte 100 in packet #%zu, packet CRC re-sealed\n", victim);

    const TransferStats s3 = send_stream(device, *v3_stream);
    tag("DATA", kTeal);
    std::printf("Received %zu/%zu DATA packets (each passed its packet CRC)\n", s3.data_ok,
                s3.data_packets);
    const bool rejected = s3.status == Status::ImageCrcMismatch && device.state() == UpdateState::Failed;
    tag("CHECK", rejected ? kRed : kAmber);
    std::printf("Image integrity: %s%s%s at %s (%s)\n", c(kRed),
                s3.status == Status::Ok ? "PASSED" : "FAILED", reset(), s3.failed_type.c_str(),
                to_string(s3.status));
    expect(rejected, "corrupted v3 image was not rejected with IMAGE_CRC_MISMATCH");

    const bool preserved = device.installed_version() == 2U && device.installed_image() == v2;
    tag("SAFE", preserved ? kGreen : kRed);
    std::printf("Installed firmware remains v%u (bytes %s), state %s\n", device.installed_version(),
                preserved ? "unchanged" : "CHANGED", to_string(device.state()));
    expect(preserved, "installed firmware changed after a failed update");

    // 12. Recover and prove the receiver is usable again.
    device.reset();
    const auto v3_clean = build_update(3, v3);
    const TransferStats s3b = v3_clean ? send_stream(device, *v3_clean) : TransferStats{};
    const bool recovered = v3_clean && s3b.status == Status::Ok && device.installed_version() == 3U &&
                           device.installed_image() == v3;
    std::printf("\n");
    tag("RESET", kTeal);
    std::printf("Receiver reset to IDLE, clean v3 retransmitted: %s%s%s -> v%u installed\n",
                c(recovered ? kGreen : kRed), recovered ? "ACCEPTED" : "REJECTED", reset(),
                device.installed_version());
    expect(recovered, "receiver did not recover after reset()");

    // 13. Final verdict from the accumulated checks.
    print_rule();
    if (g_failures == 0) {
        std::printf("%sRESULT: Update recovery and atomicity verified%s\n", c(kGreen), reset());
        return 0;
    }
    std::printf("%sRESULT: %d expectation(s) FAILED%s\n", c(kRed), g_failures, reset());
    return 1;
}
