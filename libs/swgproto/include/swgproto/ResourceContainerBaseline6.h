#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// ResourceContainer's BASE6 baseline (tag "RCNO", 0x52434E4F). Standalone:
// ResourceContainerObjectMessage6.h extends BaseLineMessage directly, not
// TangibleObjectMessage6 - confirmed from source, matching PLAN.md's
// "standalone, 7 fields" estimate. The constructor's own opCount literal
// (0x05) doesn't match the real field count (7) or the delta indices used
// (0x05/0x06, beyond a header of 5) - another Core3-source opcnt anomaly
// this project's schema engine is immune to (positional reads only).
//
// Live-verified 2026-07-17 against a real Naritus capture (56 bytes, zero
// leftover): the first 4 fields are always-empty/zero placeholders in this
// Core3 build (real capture confirms: "", 0, "", "") - the source
// constructor writes literal `insertAscii("")`/`insertInt(0)`/
// `insertAscii("")`/`UnicodeString("")` unconditionally, never populated
// from real data, so modeled as real fields (not skipped) but
// fieldBaselineOnly. maxStackSize/spawnType/resourceName carry real content
// (real capture: maxStackSize=100000, spawnType="wheat_domesticated_naboo",
// resourceName="Klebe" - matching the exact resource just spawned).
//
// spawnType (0x05, setResourceType()) and resourceName (0x06,
// setResourceName()) DO have delta setters in source, but
// ResourceContainerObjectDeltaMessage6's own constructor has BOTH calls
// commented out with the note "Causes CTD, needs research" - confirmed dead
// code, never invoked anywhere in the shipped server. Per this project's
// "Core3 is a map, not truth" principle, NOT wired as live delta fields
// (fieldBaselineOnly) despite the setter methods existing - no
// ResourceContainerObjectDeltaMessage6 traffic can legitimately occur.
struct ResourceContainerBaseline6 {
    std::string unknownField0;      // 0x00 - always "" in this build
    int32_t unknownField1 = 0;      // 0x01 - always 0 in this build
    std::string unknownField2;      // 0x02 - always "" in this build
    std::u16string containerName;   // 0x03 - always "" in this build
    int32_t maxStackSize = 0;       // 0x04 - real value (e.g. 100000)
    std::string spawnType;          // 0x05 - real value, e.g. "wheat_domesticated_naboo"
    std::u16string resourceName;    // 0x06 - real value, e.g. "Klebe"

    static ParseResult<ResourceContainerBaseline6> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kResourceContainerBaseline6Fields[] = {
    fieldBaselineOnly<FieldKind::Ascii, &ResourceContainerBaseline6::unknownField0>(
        "unknownField0"),
    fieldBaselineOnly<FieldKind::Int32, &ResourceContainerBaseline6::unknownField1>(
        "unknownField1"),
    fieldBaselineOnly<FieldKind::Ascii, &ResourceContainerBaseline6::unknownField2>(
        "unknownField2"),
    fieldBaselineOnly<FieldKind::Unicode, &ResourceContainerBaseline6::containerName>(
        "containerName"),
    fieldBaselineOnly<FieldKind::Int32, &ResourceContainerBaseline6::maxStackSize>(
        "maxStackSize"),
    fieldBaselineOnly<FieldKind::Ascii, &ResourceContainerBaseline6::spawnType>("spawnType"),
    fieldBaselineOnly<FieldKind::Unicode, &ResourceContainerBaseline6::resourceName>(
        "resourceName"),
};

inline constexpr ObjectSchema kResourceContainerBaseline6Schema{
    nullptr, 0, kResourceContainerBaseline6Fields};

} // namespace swgproto
