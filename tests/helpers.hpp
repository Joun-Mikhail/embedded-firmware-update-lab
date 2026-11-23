#pragma once

#include "fwupdate/fault_injection.hpp"
#include "fwupdate/receiver.hpp"
#include "fwupdate/sender.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace helpers {

inline constexpr std::uint32_t kV1 = 1;
inline constexpr std::uint32_t kV2 = 2;

inline std::vector<std::uint8_t> v1_image() { return fwupdate::generate_image(1000, 0x11111111U); }
inline std::vector<std::uint8_t> v2_image() { return fwupdate::generate_image(1500, 0x22222222U); }

inline fwupdate::FirmwareUpdateReceiver make_device() {
    return fwupdate::FirmwareUpdateReceiver(v1_image(), kV1);
}

// Builds a valid stream; aborts the test (via REQUIRE in the caller) if empty.
inline fwupdate::PacketStream v2_stream(std::size_t chunk = 256) {
    auto stream = fwupdate::build_update(kV2, v2_image(), chunk);
    return stream ? *stream : fwupdate::PacketStream{};
}

struct RunResult {
    fwupdate::Status status = fwupdate::Status::Ok; // first non-OK status, or OK
    std::size_t index = 0;                          // index of that packet (or stream size)
};

// Feeds packets until the first rejection.
inline RunResult feed(fwupdate::FirmwareUpdateReceiver& rx, const fwupdate::PacketStream& stream) {
    RunResult r;
    for (std::size_t i = 0; i < stream.size(); ++i) {
        const fwupdate::Status s = rx.receive(stream[i]);
        if (s != fwupdate::Status::Ok) {
            r.status = s;
            r.index = i;
            return r;
        }
    }
    r.index = stream.size();
    return r;
}

} // namespace helpers

// Asserts the project's central invariant: installed firmware untouched.
#define CHECK_INSTALLED_IS_V1(rx)                               \
    do {                                                        \
        CHECK_EQ((rx).installed_version(), helpers::kV1);       \
        CHECK((rx).installed_image() == helpers::v1_image());   \
    } while (false)
