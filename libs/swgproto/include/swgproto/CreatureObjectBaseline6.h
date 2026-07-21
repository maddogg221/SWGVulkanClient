#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/TangibleObjectBaseline6.h"
#include "swgproto/WearableEntry.h"

namespace swgproto {

// CreatureObject's BASE6 baseline (opcnt=0x16 in Core3's own
// CreatureObjectMessage6.h - not authoritative, see below; extends
// TangibleObjectMessage6, see TangibleObjectBaseline6.h). Field order
// confirmed directly from CreatureObjectMessage6.h's insert*() sequence,
// cross-checked against CreatureObjectDeltaMessage6.h's confirmed delta
// indices (0x02 level through 0x10 alternateAppearance - continuing the
// same flat index counter TangibleObjectBaseline6's 2 fields end at, 0x01).
// opcnt=22 does NOT match the 19 logical fields below (17 own + 2
// inherited) - the same "opcnt overstates the real field count" pattern
// already confirmed for PlayerObjectBaseline6; not treated as authoritative
// here either, since the delta indices and real captured byte counts are
// the actual source of truth.
struct CreatureObjectBaseline6 {
    TangibleObjectBaseline6 tangible;               // 0x00-0x01
    uint16_t level = 0;                              // 0x02
    std::string performanceAnimation;                // 0x03
    std::string moodString;                          // 0x04
    uint64_t weaponId = 0;                            // 0x05
    uint64_t groupId = 0;                             // 0x06
    // groupInviterId + groupInviteCounter - CONFIRMED to share ONE delta
    // index (0x07: CreatureObjectDeltaMessage6::updateInviterId() does
    // startUpdate(0x07) then insertLong() twice) - modeled as one member,
    // not two, matching FixedInt32x2NoTag's established precedent for
    // exactly this "multiple raw values, one shared index" shape.
    std::array<uint64_t, 2> groupInviterAndCounter{}; // 0x07
    int32_t guildId = 0;                              // 0x08
    uint64_t targetId = 0;                            // 0x09
    uint8_t moodId = 0;                                // 0x0A
    int32_t performanceStartTime = 0;                  // 0x0B
    int32_t performanceType = 0;                       // 0x0C
    // ham/maxHam/wearables are all self-serializing DeltaVectors with no
    // setter in CreatureObjectDeltaMessage6.h (indices 0x0D/0x0E/0x0F are
    // absent from its update methods) - fieldBaselineOnly, matching this
    // project's "missing is safe" policy.
    std::vector<int32_t> ham;                          // 0x0D
    std::vector<int32_t> maxHam;                       // 0x0E
    // wearables: CONFIRMED via a real captured payload (9 equipped items,
    // zero leftover bytes) to use WearablesDeltaVector's overridden 4-field
    // per-entry format (customizationString/containmentType/objectId/crc),
    // NOT a plain "ManagedReference -> uint64 ID" list - see
    // WearableEntry.h's comment for the full source citation.
    std::vector<WearableEntry> wearables;              // 0x0F
    std::string alternateAppearance;                   // 0x10
    uint8_t frozen = 0;                                // 0x11 - no confirmed delta setter

    // Parses an already-unwrapped BASE6 payload (the caller has already
    // consumed the shared BaselineEnvelope). Implemented via the schema
    // engine, decoding the ancestor (TangibleObjectBaseline6) fields first.
    static ParseResult<CreatureObjectBaseline6> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kCreatureObjectBaseline6OwnFields[] = {
    field<FieldKind::Uint16, &CreatureObjectBaseline6::level>(0x02, "level"),
    field<FieldKind::Ascii, &CreatureObjectBaseline6::performanceAnimation>(
        0x03, "performanceAnimation"),
    field<FieldKind::Ascii, &CreatureObjectBaseline6::moodString>(0x04, "moodString"),
    field<FieldKind::Uint64, &CreatureObjectBaseline6::weaponId>(0x05, "weaponId"),
    field<FieldKind::Uint64, &CreatureObjectBaseline6::groupId>(0x06, "groupId"),
    field<FieldKind::FixedUint64x2NoTag, &CreatureObjectBaseline6::groupInviterAndCounter>(
        0x07, "groupInviterAndCounter"),
    field<FieldKind::Int32, &CreatureObjectBaseline6::guildId>(0x08, "guildId"),
    field<FieldKind::Uint64, &CreatureObjectBaseline6::targetId>(0x09, "targetId"),
    field<FieldKind::Byte, &CreatureObjectBaseline6::moodId>(0x0A, "moodId"),
    field<FieldKind::Int32, &CreatureObjectBaseline6::performanceStartTime>(
        0x0B, "performanceStartTime"),
    field<FieldKind::Int32, &CreatureObjectBaseline6::performanceType>(0x0C, "performanceType"),
    fieldBaselineOnly<FieldKind::Int32Container, &CreatureObjectBaseline6::ham>("ham"),
    fieldBaselineOnly<FieldKind::Int32Container, &CreatureObjectBaseline6::maxHam>("maxHam"),
    fieldBaselineOnly<FieldKind::WearableListField, &CreatureObjectBaseline6::wearables>(
        "wearables"),
    field<FieldKind::Ascii, &CreatureObjectBaseline6::alternateAppearance>(
        0x10, "alternateAppearance"),
    fieldBaselineOnly<FieldKind::Byte, &CreatureObjectBaseline6::frozen>("frozen"),
};

inline constexpr ObjectSchema kCreatureObjectBaseline6Schema{
    &kTangibleObjectBaseline6Schema, offsetof(CreatureObjectBaseline6, tangible),
    kCreatureObjectBaseline6OwnFields};

} // namespace swgproto
