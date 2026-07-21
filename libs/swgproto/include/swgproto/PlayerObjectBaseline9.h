#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/SchematicEntry.h"

namespace swgproto {

// PlayerObject's BASE9 baseline (opCount=0x13=19 in Core3's own
// PlayerObjectMessage9.h - standalone, does NOT extend
// IntangibleObjectMessage3). Self-only, like BASE8. Field order confirmed
// directly from PlayerObjectMessage9.h's insert*() sequence, cross-checked
// against PlayerObjectDeltaMessage9.h (including its large commented-out/
// dead block, distinguished from live evidence throughout).
//
// Indices 0x0E-0x10 (unknownGap below) are a REAL, source-unresolvable
// ambiguity: between confirmed drinkFillingMax (0x0D) and confirmed
// jediState (0x11) there are exactly 4 raw int32(0) writes occupying
// exactly 3 field slots (forced arithmetic, not a guess - 0x11 is
// hard-anchored via setJediState()'s startUpdate(0x11)). WHICH of the two
// source-adjacent int pairs is "2 individual fields" vs. "1 list-shaped
// field" cannot be determined from source - modeled as ONE opaque 4-int
// blob instead of guessing at a split, matching PlayerObjectBaseline3's
// unknownTail precedent for a similar confirmed-bytes/unconfirmed-grouping
// situation.
struct PlayerObjectBaseline9 {
    std::vector<std::string> abilityList;   // 0x00 - AbilityList (activeAbilities), no null-check unlike SkillList
    int32_t experimentationEnabled = 0;     // 0x01
    int32_t craftingState = 0;              // 0x02
    uint64_t nearestCraftingStation = 0;    // 0x03 - an object-ID-shaped field (insertLong(0))
    std::vector<SchematicEntry> schematics; // 0x04
    int32_t experimentationPoints = 0;      // 0x05 - source comments it "crafting?"
    int32_t speciesData = 0;                // 0x06 - source comments it "species data"; no delta evidence found anywhere

    // 0x07/0x08: Core3 currently sends a hardcoded 2-int stub for both
    // friends/ignore lists (the real DeltaVector<String> calls are
    // commented out right above them in source) - modeled as the stub's
    // ACTUAL current wire shape, not the real-but-unused container shape.
    std::array<int32_t, 2> friendListStub{};
    std::array<int32_t, 2> ignoreListStub{};

    int32_t languageId = 0;      // 0x09
    int32_t foodFilling = 0;     // 0x0A
    int32_t foodFillingMax = 0;  // 0x0B
    int32_t drinkFilling = 0;    // 0x0C
    int32_t drinkFillingMax = 0; // 0x0D

    std::array<int32_t, 4> unknownGap{}; // 0x0E-0x10, see struct comment above

    int32_t jediState = 0; // 0x11

    // Index 0x12 (18) is declared by opcnt but never written anywhere in
    // the constructor - genuinely unpopulated, simply omitted here (no
    // struct member, no schema entry - matches this project's "missing is
    // safe" posture).

    // Parses an already-unwrapped BASE9 payload (the caller has already
    // consumed the shared BaselineEnvelope). Implemented via the schema
    // engine (see kPlayerObjectBaseline9Schema below).
    static ParseResult<PlayerObjectBaseline9> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. No ancestor (PLAY9 is standalone).
// Only the 10 fields with confirmed live delta setters are `field<>()`
// (delta-addressable): experimentationEnabled, craftingState,
// nearestCraftingStation, experimentationPoints, languageId, foodFilling,
// foodFillingMax, drinkFilling, drinkFillingMax, jediState. Everything else
// - including friendListStub/ignoreListStub, whose delta INDEX is
// independently confirmed but whose real delta SHAPE doesn't match what
// the baseline currently sends - is `fieldBaselineOnly`.
inline constexpr FieldDescriptor kPlayerObjectBaseline9Fields[] = {
    fieldBaselineOnly<FieldKind::AbilityListLike, &PlayerObjectBaseline9::abilityList>(
        "abilityList"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::experimentationEnabled>(
        0x01, "experimentationEnabled"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::craftingState>(0x02, "craftingState"),
    field<FieldKind::Uint64, &PlayerObjectBaseline9::nearestCraftingStation>(
        0x03, "nearestCraftingStation"),
    fieldBaselineOnly<FieldKind::SchematicListField, &PlayerObjectBaseline9::schematics>(
        "schematics"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::experimentationPoints>(
        0x05, "experimentationPoints"),
    fieldBaselineOnly<FieldKind::Int32, &PlayerObjectBaseline9::speciesData>("speciesData"),
    fieldBaselineOnly<FieldKind::FixedInt32x2NoTag, &PlayerObjectBaseline9::friendListStub>(
        "friendListStub"),
    fieldBaselineOnly<FieldKind::FixedInt32x2NoTag, &PlayerObjectBaseline9::ignoreListStub>(
        "ignoreListStub"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::languageId>(0x09, "languageId"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::foodFilling>(0x0A, "foodFilling"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::foodFillingMax>(0x0B, "foodFillingMax"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::drinkFilling>(0x0C, "drinkFilling"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::drinkFillingMax>(0x0D, "drinkFillingMax"),
    fieldBaselineOnly<FieldKind::FixedInt32x4NoTag, &PlayerObjectBaseline9::unknownGap>(
        "unknownGap"),
    field<FieldKind::Int32, &PlayerObjectBaseline9::jediState>(0x11, "jediState"),
};

inline constexpr ObjectSchema kPlayerObjectBaseline9Schema{nullptr, 0,
                                                             kPlayerObjectBaseline9Fields};

} // namespace swgproto
