#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// CellObject's BASE3 baseline (tag "SCLT", 0x53434C54) - one instance per
// room in a building interior. No ancestor: CellObjectMessage3/6 both
// extend BaseLineMessage directly, NOT TangibleObjectMessage3/6 (confirmed
// directly from CellObjectMessage3.h/6.h - unlike every composed schema
// this project has built so far).
//
// Source's own trailing comment claims a live capture had an EXTRA
// unaccounted byte right before cellNumber ("01 // extra bit on live?"),
// which would make this 25 bytes, not 24 - but a REAL capture taken this
// session (2 confirmed sightings, both exactly 24 bytes, zero leftover)
// shows no such byte: 20 zero bytes then a literal 4-byte cellNumber,
// matching the CURRENT live constructor's 7 insert() calls exactly
// (4+2+4+2+4+4+4=24). That old comment is stale/wrong - trust the compiled
// code and real bytes over a hand-written historical note, per this
// project's "Core3 is a map, not truth" principle applying even to Core3's
// own comments about itself.
//
// Field order confirmed from CellObjectMessage3.h's insert*() sequence.
// The two int32+int16 pairs (commented "STFName"/"STF" in source) LOOK
// like they could be a compact legacy StringId-style reference pair, but
// every real capture has them as all-zero placeholders with no non-empty
// example to confirm any such grouping - modeled here as flat scalar
// fields matching the raw wire order exactly, not guessing at a semantic
// grouping. `customName` is modeled as a real Unicode field (not an
// opaque int32) despite always being empty so far, since Core3's own
// comment explicitly calls it a name field and a room CAN be renamed by
// its owner - an opaque fixed-width field would silently misdecode a
// future non-empty capture, this won't.
//
// `cellNumber` is the ONLY confirmed delta-addressable field
// (CellObjectDeltaMessage3::updateCellNumber(), field index 0x05) - that
// index is Core3's own opaque bookkeeping constant, unrelated to this
// field's position in this struct's own field array (same as every other
// delta index in this project - never derived from local array position).
struct CellObjectBaseline3 {
    int32_t unknownField0 = 0;      // 0x00 in wire order
    uint16_t stfNameStringIndex = 0; // "STFName" per source comment
    int32_t unknownField1 = 0;
    uint16_t stfStringIndex = 0;     // "STF" per source comment
    std::u16string customName;       // "custom name" per source comment
    int32_t unknownField2 = 0;
    int32_t cellNumber = 0;          // e.g. 1, 2, 3... NOT an object ID

    static ParseResult<CellObjectBaseline3> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kCellObjectBaseline3Fields[] = {
    fieldBaselineOnly<FieldKind::Int32, &CellObjectBaseline3::unknownField0>("unknownField0"),
    fieldBaselineOnly<FieldKind::Uint16, &CellObjectBaseline3::stfNameStringIndex>(
        "stfNameStringIndex"),
    fieldBaselineOnly<FieldKind::Int32, &CellObjectBaseline3::unknownField1>("unknownField1"),
    fieldBaselineOnly<FieldKind::Uint16, &CellObjectBaseline3::stfStringIndex>("stfStringIndex"),
    fieldBaselineOnly<FieldKind::Unicode, &CellObjectBaseline3::customName>("customName"),
    fieldBaselineOnly<FieldKind::Int32, &CellObjectBaseline3::unknownField2>("unknownField2"),
    field<FieldKind::Int32, &CellObjectBaseline3::cellNumber>(0x05, "cellNumber"),
};

inline constexpr ObjectSchema kCellObjectBaseline3Schema{nullptr, 0, kCellObjectBaseline3Fields};

} // namespace swgproto
