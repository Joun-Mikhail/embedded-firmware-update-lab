#include "fwupdate/protocol.hpp"

namespace fwupdate {

const char* to_string(Status status) noexcept {
    switch (status) {
    case Status::Ok: return "OK";
    case Status::InvalidMagic: return "INVALID_MAGIC";
    case Status::UnsupportedVersion: return "UNSUPPORTED_VERSION";
    case Status::InvalidPacketType: return "INVALID_PACKET_TYPE";
    case Status::InvalidReserved: return "INVALID_RESERVED";
    case Status::InvalidLength: return "INVALID_LENGTH";
    case Status::PayloadTooLarge: return "PAYLOAD_TOO_LARGE";
    case Status::InvalidPayload: return "INVALID_PAYLOAD";
    case Status::CrcMismatch: return "CRC_MISMATCH";
    case Status::InvalidSequence: return "INVALID_SEQUENCE";
    case Status::InvalidState: return "INVALID_STATE";
    case Status::ImageTooLarge: return "IMAGE_TOO_LARGE";
    case Status::ImageIncomplete: return "IMAGE_INCOMPLETE";
    case Status::ImageCrcMismatch: return "IMAGE_CRC_MISMATCH";
    case Status::VersionRollback: return "VERSION_ROLLBACK";
    case Status::UpdateAborted: return "UPDATE_ABORTED";
    }
    return "UNKNOWN_STATUS";
}

const char* to_string(PacketType type) noexcept {
    switch (type) {
    case PacketType::Start: return "START";
    case PacketType::Data: return "DATA";
    case PacketType::Finish: return "FINISH";
    case PacketType::Abort: return "ABORT";
    }
    return "UNKNOWN_TYPE";
}

} // namespace fwupdate
