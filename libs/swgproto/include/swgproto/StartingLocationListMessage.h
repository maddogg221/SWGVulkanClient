#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

// header2 for this sub-message - confirmed via Core3's
// StartingLocationListMessage.h: ObjectControllerMessage(creo->getObjectID(),
// 0x1B, 0x1FC).
constexpr uint32_t kStartingLocationListMessageControllerType = 0x1FC;

// One entry from StartingLocation.h's insertToMessage() - insertAscii(location)
// + insertAscii(planet) + insertFloat(x) + insertFloat(y) + insertAscii(cell)
// + insertAscii(image) + insertAscii(description) + insertByte(0x01).
struct StartingLocationEntry {
    std::string location;
    std::string planet;
    float x = 0.0f;
    float y = 0.0f;
    std::string cell;
    std::string image;
    std::string description;
    uint8_t unknownFlag = 0;

    static StartingLocationEntry parse(soe::PacketBuffer& buf);
};

// The starting-city selection list sent by a StartingLocationTerminal (a
// real, admin-usable in-game object - StartingLocationTerminalImplementation
// ::handleObjectMenuSelect()). SYNTHETIC TEST ONLY - like CommandQueueAdd,
// its real trigger is a right-click radial menu selection (selectedID==20,
// "use"), which needs this project's still-undecoded SUI/radial-menu
// protocol to reach live. Unlike ObjectMenuResponse (deferred entirely -
// see KNOWN_UNKNOWNS.md), this message's own shape has zero ambiguity from
// source: StartingLocationList.h's insertToMessage() is a plain
// insertInt(count) + count * StartingLocationEntry, no offset patching or
// recursion. Implemented from source with a synthetic test, same treatment
// as CommandQueueAdd.
struct StartingLocationListMessage {
    std::vector<StartingLocationEntry> locations;

    // Parses the fields following the shared ObjControllerMessage envelope
    // (header1/header2/objectId/unused already consumed).
    static StartingLocationListMessage parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
