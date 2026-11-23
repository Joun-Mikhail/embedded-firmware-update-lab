// CRC-32/ISO-HDLC (the "zip"/Ethernet CRC-32).
//   width 32, poly 0x04C11DB7 (reflected 0xEDB88320), init 0xFFFFFFFF,
//   refin true, refout true, xorout 0xFFFFFFFF, check("123456789") = 0xCBF43926.
// CRC detects accidental corruption only. It is NOT a cryptographic
// authentication mechanism.
#pragma once

#include <cstdint>
#include <vector>

namespace fwupdate {

class Crc32 {
public:
    void update(std::uint8_t byte) noexcept;

    template <typename It>
    void update(It first, It last) noexcept {
        for (; first != last; ++first) {
            update(static_cast<std::uint8_t>(*first));
        }
    }

    // Final CRC value (xorout applied). Does not modify the running state.
    std::uint32_t value() const noexcept { return state_ ^ 0xFFFFFFFFU; }

private:
    std::uint32_t state_ = 0xFFFFFFFFU;
};

std::uint32_t crc32(const std::vector<std::uint8_t>& data) noexcept;

} // namespace fwupdate
