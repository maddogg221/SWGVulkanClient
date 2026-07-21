#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// PlayerQuestData's own toBinaryStream layout (confirmed directly from
// Core3's PlayerQuestData.h): uint64 ownerId + uint16 activeStepBitmask +
// uint16 completedStepBitmask + byte completedFlag + int32 questCounter -
// 17 bytes total.
struct PlayerQuestData {
    uint64_t ownerId = 0;
    uint16_t activeStepBitmask = 0;
    uint16_t completedStepBitmask = 0;
    uint8_t completedFlag = 0;
    uint32_t questCounter = 0;
};

// One entry from PlayerObject's playerQuestsData
// (DeltaVectorMap<uint32, PlayerQuestData> - the map key is a quest CRC).
struct PlayerQuestEntry {
    uint32_t key = 0;
    PlayerQuestData value;
};

// Parses DeltaVectorMap<uint32, PlayerQuestData>::insertToMessage's shape:
// int32 size + int32 updateCounter, then per entry: byte(0x00 ADD tag) +
// uint32 key + the 17-byte PlayerQuestData value above.
ParseResult<std::vector<PlayerQuestEntry>> parsePlayerQuestDataMap(soe::PacketBuffer& buf);

} // namespace swgproto
