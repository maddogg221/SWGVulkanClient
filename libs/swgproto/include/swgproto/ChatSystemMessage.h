#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"

namespace swgproto {

constexpr uint32_t kChatSystemMessageHash = 0x6D2A6413;

// Server-to-client system/chat message - most notably the zone-server MOTD,
// sent once per login via PlayerManagerImplementation::sendLoginMessage()
// (confirmed directly from Core3 source). Two DIFFERENT wire shapes share
// this one hash, selected by which of Core3's two constructors was used to
// build the message - this project only decodes the simpler one (a plain
// Unicode string), since that's the one the MOTD path (and every other real
// sighting captured so far - this hash has shown up as "unknown" repeatedly
// all session) uses. The other variant (a StringIdChatParameter payload, for
// localized/parameterized messages) is NOT decoded here: if it's ever
// received, `paramsSize` will be nonzero and bytes will remain after
// parse() returns, which the caller can detect and safely warn on rather
// than this project guessing at a container shape it has no real capture
// of.
struct ChatSystemMessage {
    enum DisplayType : uint8_t {
        DisplayChatAndScreen = 0x00,
        DisplayChatOnly = 0x02,
    };

    uint8_t displayType = 0;
    std::u16string message;
    // Always a literal 0 ("no params") for the simple variant this struct
    // decodes. For the StringIdChatParameter variant this would be the byte
    // size of that payload, which follows immediately after - not decoded
    // here, see the struct comment above.
    uint32_t paramsSize = 0;

    // Parses the fields following the shared opCount+hash header (already
    // consumed generically by MessageDispatcher). Wire layout: byte
    // displayType + Unicode message + uint32 paramsSize.
    static ChatSystemMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
