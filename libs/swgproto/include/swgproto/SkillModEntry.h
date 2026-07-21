#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// One entry from CreatureObject's skillMods
// (DeltaVectorMap<String, SkillModEntry> per Core3's SkillModList.h/
// SkillModEntry.h). The value's own toBinaryStream writes exactly two
// int32s (skillMod, skillBonus), no tag of its own.
struct SkillModEntry {
    std::string key;
    int32_t skillMod = 0;
    int32_t skillBonus = 0;
};

// Parses DeltaVectorMap<String,SkillModEntry>::insertToMessage's shape
// (confirmed from Core3's DeltaVectorMap.h, same header format as
// StringInt32Map/PlayerQuestData): int32 size + int32 updateCounter, then
// per entry: byte(0x00 ADD tag) + ASCII key + int32 skillMod + int32
// skillBonus.
ParseResult<std::vector<SkillModEntry>> parseSkillModList(soe::PacketBuffer& buf);

} // namespace swgproto
