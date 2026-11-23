#include "fwupdate/sender.hpp"

#include "fwupdate/crc32.hpp"
#include "fwupdate/packet.hpp"

#include <algorithm>

namespace fwupdate {
namespace {

RawPacket encode_or_empty(const Packet& packet) {
    EncodeResult encoded = encode_packet(packet);
    return encoded.status == Status::Ok ? std::move(encoded.bytes) : RawPacket{};
}

} // namespace

std::optional<PacketStream> build_update(std::uint32_t target_version,
                                         const std::vector<std::uint8_t>& image,
                                         std::size_t chunk_size) {
    if (image.empty() || image.size() > kMaxImageLength || chunk_size == 0U ||
        chunk_size > kMaxPayloadLength) {
        return std::nullopt;
    }

    PacketStream stream;
    StartInfo info;
    info.target_version = target_version;
    info.image_length = static_cast<std::uint32_t>(image.size()); // <= 64 KiB
    info.image_crc = crc32(image);
    stream.push_back(encode_or_empty(Packet{PacketType::Start, 0, encode_start_payload(info)}));

    std::uint32_t sequence = 1;
    for (std::size_t offset = 0; offset < image.size(); offset += chunk_size) {
        const std::size_t length = std::min(chunk_size, image.size() - offset);
        Packet data{PacketType::Data, sequence, {}};
        data.payload.assign(image.begin() + static_cast<std::ptrdiff_t>(offset),
                            image.begin() + static_cast<std::ptrdiff_t>(offset + length));
        stream.push_back(encode_or_empty(data));
        ++sequence;
    }
    stream.push_back(encode_or_empty(Packet{PacketType::Finish, sequence, {}}));
    return stream;
}

RawPacket build_abort(std::uint32_t sequence) {
    return encode_or_empty(Packet{PacketType::Abort, sequence, {}});
}

std::vector<std::uint8_t> generate_image(std::size_t length, std::uint32_t seed) {
    std::vector<std::uint8_t> image;
    image.reserve(length);
    std::uint32_t x = seed != 0U ? seed : 0x9E3779B9U; // xorshift32 must not start at 0
    for (std::size_t i = 0; i < length; ++i) {
        x ^= x << 13U;
        x ^= x >> 17U;
        x ^= x << 5U;
        image.push_back(static_cast<std::uint8_t>(x & 0xFFU));
    }
    return image;
}

} // namespace fwupdate
