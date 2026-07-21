#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/PlayerQuestData.h"
#include "swgproto/StringInt32Map.h"
#include "swgproto/WaypointEntry.h"

namespace swgproto {

// PlayerObject's BASE8 baseline (opCount=0x07 in Core3's own
// PlayerObjectMessage8.h - standalone, does NOT extend
// IntangibleObjectMessage3). Self-only: per
// PlayerObjectImplementation::sendBaselinesTo, only sent to the character's
// own owning client. Field order confirmed directly from
// PlayerObjectMessage8.h's insert*() sequence. Only forcePower/
// forcePowerMax have confirmed live delta setters
// (PlayerObjectDeltaMessage8.h) - every container field's delta shape is
// either dead/commented code or has no evidence at all, so none of them
// are delta-addressable (see kPlayerObjectBaseline8Schema below).
struct PlayerObjectBaseline8 {
    std::vector<StringInt32Entry> experienceList; // 0x00
    std::vector<WaypointEntry> waypointList;       // 0x01
    int32_t forcePower = 0;                        // 0x02
    int32_t forcePowerMax = 0;                     // 0x03
    std::vector<uint8_t> completedQuests;          // 0x04 - DeltaBitArray, raw bytes
    std::vector<uint8_t> activeQuests;              // 0x05 - DeltaBitArray, raw bytes
    std::vector<PlayerQuestEntry> playerQuestsData; // 0x06

    // Trailing insertInt(0); insertInt(0); - confirmed OUTSIDE the declared
    // opcnt range entirely by elimination (7 named groups above exactly
    // match indices 0-6), not even index-worthy the way unknownTail-style
    // fields elsewhere at least occupy a real index. Still consumed
    // sequentially via the schema so buf.remaining() comes out right.
    std::array<int32_t, 2> trailingUnknown{};

    // Parses an already-unwrapped BASE8 payload (the caller has already
    // consumed the shared BaselineEnvelope). Implemented via the schema
    // engine (see kPlayerObjectBaseline8Schema below).
    static ParseResult<PlayerObjectBaseline8> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. No ancestor (PLAY8 is standalone).
// experienceList/waypointList/completedQuests/activeQuests/playerQuestsData/
// trailingUnknown are all `fieldBaselineOnly` - none has a confirmed live
// delta setter (see the struct comment above); only forcePower/
// forcePowerMax are real `field<>()` (delta-addressable) entries.
inline constexpr FieldDescriptor kPlayerObjectBaseline8Fields[] = {
    fieldBaselineOnly<FieldKind::StringInt32MapField, &PlayerObjectBaseline8::experienceList>(
        "experienceList"),
    fieldBaselineOnly<FieldKind::WaypointListField, &PlayerObjectBaseline8::waypointList>(
        "waypointList"),
    field<FieldKind::Int32, &PlayerObjectBaseline8::forcePower>(0x02, "forcePower"),
    field<FieldKind::Int32, &PlayerObjectBaseline8::forcePowerMax>(0x03, "forcePowerMax"),
    fieldBaselineOnly<FieldKind::ByteContainer, &PlayerObjectBaseline8::completedQuests>(
        "completedQuests"),
    fieldBaselineOnly<FieldKind::ByteContainer, &PlayerObjectBaseline8::activeQuests>(
        "activeQuests"),
    fieldBaselineOnly<FieldKind::PlayerQuestDataMapField,
                       &PlayerObjectBaseline8::playerQuestsData>("playerQuestsData"),
    fieldBaselineOnly<FieldKind::FixedInt32x2NoTag, &PlayerObjectBaseline8::trailingUnknown>(
        "trailingUnknown"),
};

inline constexpr ObjectSchema kPlayerObjectBaseline8Schema{nullptr, 0,
                                                             kPlayerObjectBaseline8Fields};

} // namespace swgproto
