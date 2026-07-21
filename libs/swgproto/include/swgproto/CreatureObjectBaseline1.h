#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// CreatureObject's BASE1 baseline (opCount=4 in Core3's own
// CreatureObjectMessage1.h) - the credits/HAM/skills view. Field order
// confirmed directly from CreatureObjectMessage1.h's insert*() call
// sequence, cross-checked against CreatureObjectDeltaMessage1.h's delta
// field indices (0x00 bankCredits, 0x01 cashCredits) for the two scalar
// fields.
struct CreatureObjectBaseline1 {
    int32_t bankCredits = 0;
    int32_t cashCredits = 0;
    std::vector<int32_t> baseHam;       // DeltaVector<int>, see Int32DeltaContainer.h
    std::vector<std::string> skillList; // see kCreatureObjectBaseline1Schema's comment

    // Parses the fields of an already-unwrapped BASE1 payload (i.e. the
    // caller has already consumed the shared BaselineEnvelope - this reads
    // only the fields that follow it). Implemented via the schema engine
    // (see kCreatureObjectBaseline1Schema below) - PLAN.md's schema-engine
    // plan, Stage 4.
    static ParseResult<CreatureObjectBaseline1> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. No ancestor (BASE1 is standalone, does
// not extend a TangibleObject-family baseline). baseHam/skillList are
// deliberately `fieldBaselineOnly` - they still decode in sequence (their
// position in this array matters), but have no confirmed delta wire index
// (matching the old hand-written lookupField() table, which never included
// them either) so they're excluded from delta lookup entirely rather than
// given a made-up index. skillList is Core3's own SkillList type: its wire
// count covers null (skipped) entries too, so reading exactly `count`
// strings would misread past the real data - since skillList is always the
// last field here, and it's the last entry in this table, SkillListLike's
// "read until buffer exhausted" behavior (see FieldKind.h) is correct.
inline constexpr FieldDescriptor kCreatureObjectBaseline1Fields[] = {
    field<FieldKind::Int32, &CreatureObjectBaseline1::bankCredits>(0x00, "bankCredits"),
    field<FieldKind::Int32, &CreatureObjectBaseline1::cashCredits>(0x01, "cashCredits"),
    fieldBaselineOnly<FieldKind::Int32Container, &CreatureObjectBaseline1::baseHam>("baseHam"),
    fieldBaselineOnly<FieldKind::SkillListLike, &CreatureObjectBaseline1::skillList>("skillList"),
};

inline constexpr ObjectSchema kCreatureObjectBaseline1Schema{nullptr, 0,
                                                               kCreatureObjectBaseline1Fields};

} // namespace swgproto
