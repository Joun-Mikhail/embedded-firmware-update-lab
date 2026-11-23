#include "fwupdate/receiver.hpp"

#include "fwupdate/crc32.hpp"

#include <utility>

namespace fwupdate {

const char* to_string(UpdateState state) noexcept {
    switch (state) {
    case UpdateState::Idle: return "IDLE";
    case UpdateState::Receiving: return "RECEIVING";
    case UpdateState::Verified: return "VERIFIED";
    case UpdateState::Failed: return "FAILED";
    }
    return "UNKNOWN_STATE";
}

FirmwareUpdateReceiver::FirmwareUpdateReceiver(std::vector<std::uint8_t> installed_image,
                                               std::uint32_t installed_version)
    : installed_image_(std::move(installed_image)), installed_version_(installed_version) {}

Status FirmwareUpdateReceiver::receive(const std::vector<std::uint8_t>& raw_packet) {
    if (state_ == UpdateState::Verified || state_ == UpdateState::Failed) {
        // Terminal states: nothing is decoded or changed until reset().
        last_status_ = Status::InvalidState;
        return last_status_;
    }
    const DecodeResult decoded = decode_packet(raw_packet);
    if (decoded.status != Status::Ok) {
        return reject(decoded.status);
    }
    return handle(decoded.packet);
}

Status FirmwareUpdateReceiver::handle(const Packet& packet) {
    if (state_ == UpdateState::Verified || state_ == UpdateState::Failed) {
        last_status_ = Status::InvalidState;
        return last_status_;
    }
    // Packets handed in directly must satisfy the same structural rules that
    // decode_packet() enforces for bytes from the wire.
    const Status structural = validate_payload_for_type(packet.type, packet.payload.size());
    if (structural != Status::Ok) {
        return reject(structural);
    }
    switch (packet.type) {
    case PacketType::Start: return handle_start(packet);
    case PacketType::Data: return handle_data(packet);
    case PacketType::Finish: return handle_finish(packet);
    case PacketType::Abort: return handle_abort(packet);
    }
    return reject(Status::InvalidPacketType);
}

void FirmwareUpdateReceiver::reset() noexcept {
    clear_session();
    state_ = UpdateState::Idle;
    last_status_ = Status::Ok;
}

Status FirmwareUpdateReceiver::handle_start(const Packet& packet) {
    if (state_ != UpdateState::Idle) {
        return reject(Status::InvalidState);
    }
    if (packet.sequence != 0U) {
        return reject(Status::InvalidSequence);
    }
    const std::optional<StartInfo> info = parse_start_payload(packet.payload);
    if (!info) {
        return reject(Status::InvalidPayload);
    }
    if (info->image_length == 0U) {
        return reject(Status::InvalidLength);
    }
    if (info->image_length > kMaxImageLength) {
        return reject(Status::ImageTooLarge);
    }
    if (info->target_version <= installed_version_) {
        return reject(Status::VersionRollback);
    }

    session_ = *info;
    staging_.clear();
    staging_.reserve(info->image_length);
    next_sequence_ = 1;
    state_ = UpdateState::Receiving;
    last_status_ = Status::Ok;
    return last_status_;
}

Status FirmwareUpdateReceiver::handle_data(const Packet& packet) {
    if (state_ != UpdateState::Receiving) {
        return reject(Status::InvalidState);
    }
    if (packet.sequence != next_sequence_) {
        return reject(Status::InvalidSequence);
    }
    // staging_.size() <= session_.image_length is a class invariant, so the
    // subtraction cannot wrap.
    const std::size_t remaining = static_cast<std::size_t>(session_.image_length) - staging_.size();
    if (packet.payload.size() > remaining) {
        return reject(Status::ImageTooLarge);
    }
    staging_.insert(staging_.end(), packet.payload.begin(), packet.payload.end());
    // Cannot overflow: each DATA carries >= 1 byte and the image is <= 64 KiB.
    ++next_sequence_;
    last_status_ = Status::Ok;
    return last_status_;
}

Status FirmwareUpdateReceiver::handle_finish(const Packet& packet) {
    if (state_ != UpdateState::Receiving) {
        return reject(Status::InvalidState);
    }
    if (packet.sequence != next_sequence_) {
        return reject(Status::InvalidSequence);
    }
    if (staging_.size() != session_.image_length) {
        return reject(Status::ImageIncomplete);
    }
    if (crc32(staging_) != session_.image_crc) {
        return reject(Status::ImageCrcMismatch);
    }

    // Commit: image and version change together, after every check passed.
    // vector::swap is noexcept, so the commit cannot be half-applied.
    installed_image_.swap(staging_);
    installed_version_ = session_.target_version;
    clear_session();
    state_ = UpdateState::Verified;
    last_status_ = Status::Ok;
    return last_status_;
}

Status FirmwareUpdateReceiver::handle_abort(const Packet& packet) {
    if (state_ != UpdateState::Receiving) {
        return reject(Status::InvalidState);
    }
    if (packet.sequence != next_sequence_) {
        return reject(Status::InvalidSequence);
    }
    clear_session();
    state_ = UpdateState::Idle;
    last_status_ = Status::UpdateAborted;
    return last_status_;
}

Status FirmwareUpdateReceiver::reject(Status reason) {
    clear_session();
    state_ = UpdateState::Failed;
    last_status_ = reason;
    return reason;
}

void FirmwareUpdateReceiver::clear_session() noexcept {
    staging_.clear();
    session_ = StartInfo{};
    next_sequence_ = 0;
}

} // namespace fwupdate
