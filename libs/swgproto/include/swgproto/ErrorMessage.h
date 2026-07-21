#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

// hashCode("ErrorMessage") - Core3's own source even has this value in a
// comment (ErrorMessage.h), confirmed against a real Finalizer rejection.
constexpr uint32_t kErrorMessageHash = 0xB5ABF91A;

struct ErrorMessage {
    std::string errorType;
    std::string errorMsg;
    uint8_t fatal = 0;

    // Parses the fields following opCount+hash. Wire layout: ASCII
    // errorType + ASCII errorMsg + byte fatal.
    static ErrorMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
