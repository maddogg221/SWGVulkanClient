#pragma once

#include <cstdint>
#include <vector>

namespace swgproto {

// Builds the opCount(2)+hash(4)+fields payload for SelectCharacter, ready to
// hand to SoeSession::sendMessage(). Wire layout per Core3's own source:
// uint64 characterID + int32 hash("SWGEmu"). Unlike AccountVersionMessage's
// analogous trailing field (proven unnecessary by a real packet capture -
// see DISCOVERY.txt), we have no capture evidence for SelectCharacter, so
// this includes the trailing field to match Core3's source exactly; an
// extra field the server doesn't read is harmless, but omitting a field it
// does read would break zone-in.
std::vector<uint8_t> buildSelectCharacter(uint64_t characterId);

} // namespace swgproto
