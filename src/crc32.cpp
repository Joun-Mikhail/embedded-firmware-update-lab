#include "fwupdate/crc32.hpp"

#include <array>

namespace fwupdate {
namespace {

constexpr std::uint32_t kReflectedPolynomial = 0xEDB88320U;

constexpr std::array<std::uint32_t, 256> make_table() {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t i = 0; i < 256U; ++i) {
        std::uint32_t c = i;
        for (int bit = 0; bit < 8; ++bit) {
            c = (c & 1U) != 0U ? (c >> 1U) ^ kReflectedPolynomial : (c >> 1U);
        }
        table[i] = c;
    }
    return table;
}

constexpr std::array<std::uint32_t, 256> kTable = make_table();

} // namespace

void Crc32::update(std::uint8_t byte) noexcept {
    const std::uint32_t index = (state_ ^ byte) & 0xFFU;
    state_ = (state_ >> 8U) ^ kTable[index];
}

std::uint32_t crc32(const std::vector<std::uint8_t>& data) noexcept {
    Crc32 crc;
    crc.update(data.begin(), data.end());
    return crc.value();
}

} // namespace fwupdate
