#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"

namespace swgproto {

// hashCode("EnumerateCharacterId") - a computed message hash. Verified
// 0x65EA4574 against a real captured Finalizer response.
constexpr uint32_t kEnumerateCharacterIdHash = 0x65EA4574;

struct CharacterEntry {
    std::u16string name;
    uint32_t race = 0; // race template CRC
    uint64_t objectId = 0;
    uint32_t galaxyId = 0;
    uint32_t characterType = 0; // 1 = regular, 2 = pre-Pub9 Jedi
};

struct EnumerateCharacterId {
    std::vector<CharacterEntry> characters;

    // Parses the fields following opCount+hash. Wire layout: uint32
    // characterCount, then per character: Unicode name + uint32 race +
    // uint64 objectId + uint32 galaxyId + uint32 characterType.
    static EnumerateCharacterId parse(soe::PacketBuffer& buf);
};

} // namespace swgproto
