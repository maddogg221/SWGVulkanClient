#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/TangibleObjectBaseline3.h"

namespace swgproto {

// CreatureObject's BASE3 baseline (opCount=0x12 in Core3's own
// CreatureObjectMessage3.h, extending TangibleObjectMessage3 - see
// TangibleObjectBaseline3.h). Field order confirmed directly from
// CreatureObjectMessage3.h's insert*() sequence, cross-checked against
// CreatureObjectDeltaMessage3.h's delta field indices (0x0B posture
// through 0x11 wounds - continuing the SAME flat index counter
// TangibleObjectBaseline3's fields end at, 0x0A).
struct CreatureObjectBaseline3 {
    TangibleObjectBaseline3 tangible; // 0x00-0x0A
    uint8_t posture = 0;              // 0x0B
    uint8_t factionRank = 0;          // 0x0C
    uint64_t creatureLinkId = 0;      // 0x0D
    float height = 0.0f;              // 0x0E
    int32_t shockWounds = 0;          // 0x0F
    uint64_t stateBitmask = 0;        // 0x10
    std::vector<int32_t> wounds;      // 0x11

    // Parses an already-unwrapped BASE3 payload (the caller has already
    // consumed the shared BaselineEnvelope). Implemented via the schema
    // engine (see kCreatureObjectBaseline3Schema below), which decodes the
    // ancestor (TangibleObjectBaseline3) fields first, mirroring the C++
    // composition above - PLAN.md's schema-engine plan, Stage 4.
    static ParseResult<CreatureObjectBaseline3> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. `ancestor` points at
// TangibleObjectBaseline3's own schema (migrated in Stage 2) - this is the
// first composed schema in the migration, exercising ObjectSchema's
// ancestor/ancestorOffset mechanism for real.
inline constexpr FieldDescriptor kCreatureObjectBaseline3OwnFields[] = {
    field<FieldKind::Byte, &CreatureObjectBaseline3::posture>(0x0B, "posture"),
    field<FieldKind::Byte, &CreatureObjectBaseline3::factionRank>(0x0C, "factionRank"),
    field<FieldKind::Uint64, &CreatureObjectBaseline3::creatureLinkId>(0x0D, "creatureLinkId"),
    field<FieldKind::Float, &CreatureObjectBaseline3::height>(0x0E, "height"),
    field<FieldKind::Int32, &CreatureObjectBaseline3::shockWounds>(0x0F, "shockWounds"),
    field<FieldKind::Uint64, &CreatureObjectBaseline3::stateBitmask>(0x10, "stateBitmask"),
    field<FieldKind::Int32Container, &CreatureObjectBaseline3::wounds>(0x11, "wounds"),
};

inline constexpr ObjectSchema kCreatureObjectBaseline3Schema{
    &kTangibleObjectBaseline3Schema, offsetof(CreatureObjectBaseline3, tangible),
    kCreatureObjectBaseline3OwnFields};

} // namespace swgproto
