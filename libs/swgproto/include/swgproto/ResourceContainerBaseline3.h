#pragma once

#include <cstddef>
#include <cstdint>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/TangibleObjectBaseline3.h"

namespace swgproto {

// ResourceContainer's BASE3 baseline (tag "RCNO", 0x52434E4F). Extends
// TangibleObjectMessage3 (calls its constructor first, so its own 11 fields -
// see TangibleObjectBaseline3.h - are written unchanged) and appends exactly
// 2 own fields: quantity/"Stack Size" (int32) and resourceId/"ResourceID"
// (uint64), confirmed from ResourceContainerObjectMessage3.h's insert*()
// sequence. The constructor's own opCount literal (0x0F=15) doesn't match
// the real total field count (11 inherited + 2 own = 13) - same class of
// Core3 quirk as InstallationObject's understating opcnt; this project's
// schema engine never trusts opcnt, only reads fields positionally.
//
// Live-verified 2026-07-17 against a real Naritus capture (spawned via
// `/object createresource Klebe 500`): 133 bytes, zero leftover, quantity=500
// (matching the requested amount), resourceId=5629499551006507 (a real
// ResourceSpawn ID).
//
// `quantity`'s delta index (0x0B) is confirmed live from
// ResourceContainerObjectDeltaMessage3.h's updateQuantity(), and IS actually
// called (ResourceContainerImplementation::setQuantity() broadcasts it) -
// wired as a real delta field despite no live delta capture yet (same
// "simple + literal-confirmed" precedent as InstallationObject's
// activeFlag). `resourceId` has no live delta path: the delta message class
// has a commented-out, uncalled setResourceID(0x0E) - dead code, not wired
// (matches this project's "Core3 is a map, not truth" principle).
struct ResourceContainerBaseline3 {
    TangibleObjectBaseline3 tangible; // 0x00-0x0A
    int32_t quantity = 0;             // 0x0B - "Stack Size"
    uint64_t resourceId = 0;          // no confirmed live delta path

    static ParseResult<ResourceContainerBaseline3> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kResourceContainerBaseline3OwnFields[] = {
    field<FieldKind::Int32, &ResourceContainerBaseline3::quantity>(0x0B, "quantity"),
    fieldBaselineOnly<FieldKind::Uint64, &ResourceContainerBaseline3::resourceId>("resourceId"),
};

inline constexpr ObjectSchema kResourceContainerBaseline3Schema{
    &kTangibleObjectBaseline3Schema, offsetof(ResourceContainerBaseline3, tangible),
    kResourceContainerBaseline3OwnFields};

} // namespace swgproto
