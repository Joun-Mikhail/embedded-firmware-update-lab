// Firmware update receiver: an explicit state machine with a staging buffer and
// an installed-firmware slot that changes only on a fully verified commit.
#pragma once

#include "fwupdate/packet.hpp"
#include "fwupdate/protocol.hpp"

#include <cstdint>
#include <vector>

namespace fwupdate {

enum class UpdateState : std::uint8_t {
    Idle,      // no update active; accepts START only
    Receiving, // START accepted; accepts DATA, FINISH, ABORT
    Verified,  // last update committed; reset() required before the next one
    Failed,    // last update rejected; reset() required before the next one
};

const char* to_string(UpdateState state) noexcept;

class FirmwareUpdateReceiver {
public:
    FirmwareUpdateReceiver(std::vector<std::uint8_t> installed_image,
                           std::uint32_t installed_version);

    // Decodes raw bytes and processes the packet. A decode error is handled
    // exactly like a rejected packet (see docs/state_machine.md).
    Status receive(const std::vector<std::uint8_t>& raw_packet);

    // Processes an already-decoded packet.
    Status handle(const Packet& packet);

    // Returns to IDLE from any state and discards staged data. The installed
    // image and version are never modified by reset().
    void reset() noexcept;

    UpdateState state() const noexcept { return state_; }
    std::uint32_t installed_version() const noexcept { return installed_version_; }
    const std::vector<std::uint8_t>& installed_image() const noexcept { return installed_image_; }
    std::size_t staged_size() const noexcept { return staging_.size(); }
    std::uint32_t next_sequence() const noexcept { return next_sequence_; }
    Status last_status() const noexcept { return last_status_; }

private:
    Status handle_start(const Packet& packet);
    Status handle_data(const Packet& packet);
    Status handle_finish(const Packet& packet);
    Status handle_abort(const Packet& packet);
    Status reject(Status reason);
    void clear_session() noexcept;

    std::vector<std::uint8_t> installed_image_;
    std::uint32_t installed_version_;

    UpdateState state_ = UpdateState::Idle;
    std::vector<std::uint8_t> staging_;
    StartInfo session_{};
    std::uint32_t next_sequence_ = 0;
    Status last_status_ = Status::Ok;
};

} // namespace fwupdate
