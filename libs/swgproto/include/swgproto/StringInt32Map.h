#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from a DeltaVectorMap<String, int> (e.g. PlayerObject's
// experienceList - skill/xp-type name to amount).
struct StringInt32Entry {
    std::string key;
    int32_t value = 0;
};

// Parses DeltaVectorMap<String,int>::insertToMessage's shape (confirmed
// directly from Core3's DeltaVectorMap.h): int32 size + int32 updateCounter,
// then per entry: byte(0x00, the DeltaMapCommands::ADD tag - read and
// validated but not stored, since this project models "what does the
// baseline currently say", not live add/remove semantics) + ASCII key +
// int32 value.
ParseResult<std::vector<StringInt32Entry>> parseStringInt32Map(soe::PacketBuffer& buf);

} // namespace swgproto
