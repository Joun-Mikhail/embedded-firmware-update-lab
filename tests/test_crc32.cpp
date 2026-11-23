#include "fwupdate/crc32.hpp"
#include "testkit.hpp"

#include <string>

using fwupdate::crc32;

namespace {
std::vector<std::uint8_t> bytes_of(const std::string& s) { return {s.begin(), s.end()}; }
} // namespace

FW_TEST(PKT_002_crc32_known_answer, "CRC-32/ISO-HDLC check value for \"123456789\" is 0xCBF43926") {
    CHECK_EQ(crc32(bytes_of("123456789")), 0xCBF43926U);
}

FW_TEST(CRC_empty_input, "CRC-32 of zero-length input is 0x00000000") {
    CHECK_EQ(crc32({}), 0x00000000U);
}

FW_TEST(CRC_binary_vectors, "CRC-32 of representative binary inputs matches reference values") {
    // Reference values computed independently with Python's zlib.crc32.
    CHECK_EQ(crc32({0x00}), 0xD202EF8DU);
    CHECK_EQ(crc32({0xFF, 0xFF, 0xFF, 0xFF}), 0xFFFFFFFFU);
    CHECK_EQ(crc32(std::vector<std::uint8_t>(32, 0x00)), 0x190A55ADU);
    CHECK_EQ(crc32(bytes_of("The quick brown fox jumps over the lazy dog")), 0x414FA339U);
}

FW_TEST(CRC_incremental_equals_one_shot, "Streaming CRC over chunks equals one-shot CRC") {
    const auto data = bytes_of("123456789");
    fwupdate::Crc32 c;
    c.update(data.begin(), data.begin() + 4);
    c.update(data.begin() + 4, data.end());
    CHECK_EQ(c.value(), 0xCBF43926U);
}
