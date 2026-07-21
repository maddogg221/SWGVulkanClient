#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/MissionObjectEntry.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/SkillModEntry.h"

namespace swgproto {

// CreatureObject's BASE4 baseline (opCount=0x0E in Core3's own
// CreatureObjectMessage4.h) - movement/encumbrance modifiers. No
// TangibleObject ancestor: TangibleObject has no BASE4 file, so this is
// standalone like BASE1 (confirmed - no TangibleObjectMessage4.h exists in
// source). Field order confirmed directly from CreatureObjectMessage4.h's
// insert*() call sequence, cross-checked against
// CreatureObjectDeltaMessage4.h for delta indices 0x00-0x0C.
//
// encumbrances (0x02) and skillMods (0x03) have no confirmed delta wire
// index THIS PROJECT MODELS: skillMods actually IS delta-addressable in
// source (CreatureObjectImplementation::addSkillMod, index 0x03, ADD/SET/
// DROP commands nested inside the field update), but that's a materially
// different delta shape from every other field this project decodes
// (a whole list-command body inside one field update, not a single scalar
// or single container replace) - deferred until real traffic actually
// exercises it, per "Core3 is a map, not truth". encumbrances has no delta
// setter anywhere in source at all. spaceMissionObjects (0x0D) is
// space/JTL-only content (DeltaSet<uint64,uint64>, delta-addressable via
// insertKeyAndValuesToMessage) - deferred for the same reason plus low
// expected real-world frequency on a ground-game capture.
struct CreatureObjectBaseline4 {
    float accelerationMultiplierBase = 0.0f;    // 0x00
    float accelerationMultiplierMod = 0.0f;     // 0x01
    std::vector<int32_t> encumbrances;          // 0x02, DeltaVector<int>
    std::vector<SkillModEntry> skillMods;       // 0x03
    float speedMultiplierBase = 0.0f;           // 0x04
    float speedMultiplierMod = 0.0f;            // 0x05
    uint64_t listenId = 0;                      // 0x06, "Listen to ID" (Entertainer)
    float runSpeed = 0.0f;                      // 0x07
    float slopeModAngle = 0.0f;                 // 0x08
    float slopeModPercent = 0.0f;                // 0x09
    float turnScale = 0.0f;                      // 0x0A
    float walkSpeed = 0.0f;                      // 0x0B
    float waterModPercent = 0.0f;                // 0x0C
    std::vector<MissionObjectEntry> spaceMissionObjects; // 0x0D

    static ParseResult<CreatureObjectBaseline4> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kCreatureObjectBaseline4Fields[] = {
    field<FieldKind::Float, &CreatureObjectBaseline4::accelerationMultiplierBase>(
        0x00, "accelerationMultiplierBase"),
    field<FieldKind::Float, &CreatureObjectBaseline4::accelerationMultiplierMod>(
        0x01, "accelerationMultiplierMod"),
    fieldBaselineOnly<FieldKind::Int32Container, &CreatureObjectBaseline4::encumbrances>(
        "encumbrances"),
    fieldBaselineOnly<FieldKind::SkillModListField, &CreatureObjectBaseline4::skillMods>(
        "skillMods"),
    field<FieldKind::Float, &CreatureObjectBaseline4::speedMultiplierBase>(0x04,
                                                                            "speedMultiplierBase"),
    field<FieldKind::Float, &CreatureObjectBaseline4::speedMultiplierMod>(0x05,
                                                                           "speedMultiplierMod"),
    field<FieldKind::Uint64, &CreatureObjectBaseline4::listenId>(0x06, "listenId"),
    field<FieldKind::Float, &CreatureObjectBaseline4::runSpeed>(0x07, "runSpeed"),
    field<FieldKind::Float, &CreatureObjectBaseline4::slopeModAngle>(0x08, "slopeModAngle"),
    field<FieldKind::Float, &CreatureObjectBaseline4::slopeModPercent>(0x09, "slopeModPercent"),
    field<FieldKind::Float, &CreatureObjectBaseline4::turnScale>(0x0A, "turnScale"),
    field<FieldKind::Float, &CreatureObjectBaseline4::walkSpeed>(0x0B, "walkSpeed"),
    field<FieldKind::Float, &CreatureObjectBaseline4::waterModPercent>(0x0C, "waterModPercent"),
    fieldBaselineOnly<FieldKind::MissionObjectListField,
                       &CreatureObjectBaseline4::spaceMissionObjects>("spaceMissionObjects"),
};

inline constexpr ObjectSchema kCreatureObjectBaseline4Schema{nullptr, 0,
                                                               kCreatureObjectBaseline4Fields};

} // namespace swgproto
