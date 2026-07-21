#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// CellObject's BASE6 baseline (tag "SCLT"). No ancestor (same as BASE3 -
// extends BaseLineMessage directly). No CellObjectDeltaMessage6.h exists
// anywhere in source at all, so NOTHING in this baseline is delta-
// addressable - every field is fieldBaselineOnly.
//
// Source's own trailing comments disagree with the CURRENT live code on
// the first field's value (stale notes show 0x42 and 0x8C from older Core3
// versions; the compiled constructor writes 0x95) - a real capture taken
// this session (2 confirmed sightings, both exactly 12 bytes) shows 0x95,
// matching the current live-coded constructor exactly, confirming (once
// again) that Core3's own historical comments aren't trustworthy, only
// the currently-compiled code and real wire bytes are.
//
// The trailing 8 bytes (2 raw int32 inserts, always 0/0 in every real
// capture) are modeled as an Int32Container (count+updateCounter+N items) -
// the same count+updateCounter-shaped trailer TangibleObjectBaseline6's
// `defenders` field already uses - rather than 2 flat opaque scalars,
// since it decodes byte-identically for the always-empty case observed so
// far but would also correctly handle a hypothetical non-empty list
// without needing a future revisit. Purely a safety/consistency choice,
// not confirmed against a non-empty real example (there is no delta
// message class to cross-reference either, unlike TANO BASE6's own
// still-unverified `defenders` per-item shape).
struct CellObjectBaseline6 {
    int32_t unknownConst = 0;        // 0x00 - literal 0x95 (149) per source
    std::vector<int32_t> unknownList; // count+updateCounter+N-int32-items, always empty so far

    static ParseResult<CellObjectBaseline6> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kCellObjectBaseline6Fields[] = {
    fieldBaselineOnly<FieldKind::Int32, &CellObjectBaseline6::unknownConst>("unknownConst"),
    fieldBaselineOnly<FieldKind::Int32Container, &CellObjectBaseline6::unknownList>(
        "unknownList"),
};

inline constexpr ObjectSchema kCellObjectBaseline6Schema{nullptr, 0, kCellObjectBaseline6Fields};

} // namespace swgproto
