#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/IntangibleObjectBaseline3.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// PlayerObject's BASE3 baseline (opCount=0x0B in Core3's own
// PlayerObjectMessage3.h, extending IntangibleObjectMessage3 - see
// IntangibleObjectBaseline3.h). Field order confirmed directly from
// PlayerObjectMessage3.h's insert*() sequence, cross-checked against
// PlayerObjectDeltaMessage3.h's delta field indices (0x05 playerBitmask
// through 0x09 totalPlayedTime - continuing the SAME flat index counter
// IntangibleObjectBaseline3's fields end at, 0x04).
struct PlayerObjectBaseline3 {
    IntangibleObjectBaseline3 intangible; // 0x00-0x04

    // 0x05/0x06: NOT the Int32Container shape - Core3 writes a literal
    // count (always 4 in source) followed directly by 4 raw int32s, with
    // NO updateCounter field in between. Fixed-size on the wire either way,
    // so modeled as a fixed array rather than a vector.
    std::array<int32_t, 4> playerBitmask{};   // 0x05
    std::array<int32_t, 4> profileSettings{}; // 0x06 - always 4 zeros in source today

    std::string title;             // 0x07
    int32_t birthDate = 0;         // 0x08
    int32_t totalPlayedTime = 0;   // 0x09

    // 0x0A: 3 raw int32s with literal constant values in source
    // (0x6C2/0xDC62/0x23) and no delta setter anywhere - position/bytes
    // are confirmed, semantic meaning is not. Kept as raw ints rather than
    // named fields for that reason, and excluded from delta lookup
    // entirely (see kPlayerObjectBaseline3Schema's comment).
    std::array<int32_t, 3> unknownTail{};

    // Parses the fields following a BaselineEnvelope already consumed by
    // the caller. Implemented via the schema engine (see
    // kPlayerObjectBaseline3Schema below), which decodes the ancestor
    // (IntangibleObjectBaseline3) fields first, mirroring the C++
    // composition above - PLAN.md's schema-engine plan, Stage 5.
    static ParseResult<PlayerObjectBaseline3> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. `ancestor` points at
// IntangibleObjectBaseline3's own schema (migrated in Stage 3). unknownTail
// is `fieldBaselineOnly` - it still decodes in sequence (it's the last
// field), but has no confirmed delta wire index or grouping (one field vs
// three - source doesn't say), so it's excluded from delta lookup entirely
// rather than given a made-up index, same reasoning as
// CreatureObjectBaseline1's baseHam/skillList.
inline constexpr FieldDescriptor kPlayerObjectBaseline3OwnFields[] = {
    field<FieldKind::FixedInt32x4, &PlayerObjectBaseline3::playerBitmask>(0x05, "playerBitmask"),
    field<FieldKind::FixedInt32x4, &PlayerObjectBaseline3::profileSettings>(0x06,
                                                                             "profileSettings"),
    field<FieldKind::Ascii, &PlayerObjectBaseline3::title>(0x07, "title"),
    field<FieldKind::Int32, &PlayerObjectBaseline3::birthDate>(0x08, "birthDate"),
    field<FieldKind::Int32, &PlayerObjectBaseline3::totalPlayedTime>(0x09, "totalPlayedTime"),
    fieldBaselineOnly<FieldKind::FixedInt32x3NoTag, &PlayerObjectBaseline3::unknownTail>(
        "unknownTail"),
};

inline constexpr ObjectSchema kPlayerObjectBaseline3Schema{
    &kIntangibleObjectBaseline3Schema, offsetof(PlayerObjectBaseline3, intangible),
    kPlayerObjectBaseline3OwnFields};

} // namespace swgproto
