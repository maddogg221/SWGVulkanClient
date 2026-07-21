#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/GroupObjectEntry.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// GroupObject's BASE6 baseline (tag "GRUP", 0x47525550). Standalone, no
// ancestor. Field order confirmed directly from GroupObjectMessage6.h's
// insert*() sequence, and now confirmed byte-for-byte against a real live
// capture (106/106 bytes, zero leftover): int(0) [unknown, no delta index],
// insertMembers() (the two list blocks below), ascii("") [always-empty
// "Group Name"], short(getGroupLevel()), int(0) [Formation Name CRC, no
// confirmed live delta], long(getMasterLooterID()), int(getLootRule()).
//
// `members`/`ships` are Core3's own hand-rolled "DeltaVector" shape, NOT the
// DeltaVectorMap convention (ADD=0/DROP=1/SET=2) used elsewhere in this
// project - GroupObject's delta tag scheme is ADD=1/UPDATE=2/REMOVE=0/
// CLEAR=4 instead (see GroupObjectDelta6.h). The BASELINE entries here carry
// no per-entry tag at all though (see GroupObjectEntry.h) - only the delta
// path is tagged.
//
// A real, source-confirmed quirk kept faithfully rather than "fixed": the
// ship list's own count/updateCounter header is written fresh on the wire,
// but Core3 populates it FROM the member list's size/counter, not the ship
// list's own - `parseGroupShipList` just reads whatever ints are actually
// there, so this composes correctly without needing to know about the quirk
// at parse time.
//
// This ENTIRE type bypasses SchemaEngine's generic delta path (every field
// below is fieldBaselineOnly) - see GroupObjectDelta6.h for why: indices
// 0x01/0x02 mutate an EXISTING list incrementally via per-entry ADD/UPDATE/
// REMOVE/CLEAR tags, a fundamentally different contract than every other
// FieldKind's "read one complete fresh value" semantics that
// SchemaEngine::applyDeltaMessage relies on. Since the very first field in
// every real delta is 0x01 (members), letting it hit the generic engine's
// "unmapped index -> stop early" fallback would silently drop the ENTIRE
// delta, including the simple scalar fields (groupLevel/masterLooterId/
// lootRule) that would otherwise decode fine on their own - so this type's
// delta is handled by one dedicated, non-generic function instead of a
// partial/confusing mix of the two mechanisms.
struct GroupObjectBaseline6 {
    int32_t unknownField1 = 0; // always 0, no delta index
    std::vector<GroupMemberEntry> members;
    std::vector<GroupShipEntry> ships;
    std::string groupName;         // always empty (delta index 0x03, confirmed live but always "")
    uint16_t groupLevel = 0;       // delta index 0x04
    int32_t formationNameCrc = 0;  // always 0, no confirmed live delta
    uint64_t masterLooterId = 0;   // delta index 0x06
    int32_t lootRule = 0;          // delta index 0x07 (0=Free4All/1=MasterLooter/2=Lottery/3=Random)

    static ParseResult<GroupObjectBaseline6> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kGroupObjectBaseline6Fields[] = {
    fieldBaselineOnly<FieldKind::Int32, &GroupObjectBaseline6::unknownField1>("unknownField1"),
    fieldBaselineOnly<FieldKind::GroupMemberListField, &GroupObjectBaseline6::members>("members"),
    fieldBaselineOnly<FieldKind::GroupShipListField, &GroupObjectBaseline6::ships>("ships"),
    fieldBaselineOnly<FieldKind::Ascii, &GroupObjectBaseline6::groupName>("groupName"),
    fieldBaselineOnly<FieldKind::Uint16, &GroupObjectBaseline6::groupLevel>("groupLevel"),
    fieldBaselineOnly<FieldKind::Int32, &GroupObjectBaseline6::formationNameCrc>(
        "formationNameCrc"),
    fieldBaselineOnly<FieldKind::Uint64, &GroupObjectBaseline6::masterLooterId>("masterLooterId"),
    fieldBaselineOnly<FieldKind::Int32, &GroupObjectBaseline6::lootRule>("lootRule"),
};

inline constexpr ObjectSchema kGroupObjectBaseline6Schema{nullptr, 0, kGroupObjectBaseline6Fields};

} // namespace swgproto
