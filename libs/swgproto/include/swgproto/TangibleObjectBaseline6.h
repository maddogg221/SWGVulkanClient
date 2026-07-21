#pragma once

#include <cstdint>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// TangibleObject's BASE6 baseline fields - the ancestor layer
// CreatureObjectMessage6 (see CreatureObjectBaseline6.h) extends. Field
// order confirmed directly from Core3's TangibleObjectMessage6.h insert*()
// sequence: a literal int32 constant (always 0x76 in source - the header
// comment "0x3D in creos" doesn't match the actual code path, which never
// branches; CreatureObjectMessage6 extends this constructor unchanged, so
// the wire value is 0x76 in both contexts unless live traffic says
// otherwise) followed by the defenders list. TangibleObjectDeltaMessage6.h
// is a confirmed EMPTY class (no update methods at all) - neither field has
// any delta setter, so both are fieldBaselineOnly here.
struct TangibleObjectBaseline6 {
    int32_t unknownConst = 0; // 0x00 - literal 0x76 per source
    // defenders: DeltaVector<ManagedReference<SceneObject*>>, a PLAIN
    // DeltaVector with no custom subclass override (unlike
    // CreatureObjectBaseline6's wearables, which turned out to need one -
    // see WearableEntry.h). Modeled as a plain uint64-per-entry list
    // (the generic TypeInfo<ManagedReference<T*>> serialization), but this
    // is UNVERIFIED against real bytes - every real capture so far has an
    // empty defender list (count=0), so the per-item format has never
    // actually been exercised. Safe either way: an empty list decodes
    // identically regardless, and this field is fieldBaselineOnly (no
    // delta path), so a wrong assumption would only ever surface as a
    // leftover-bytes warning on that one object's baseline, not a crash.
    std::vector<uint64_t> defenders; // 0x01

    // Parses these 2 fields from an already-unwrapped BASE6 payload (the
    // caller has already consumed the shared BaselineEnvelope). Does NOT
    // consume anything beyond these 2 fields - CreatureObjectBaseline6
    // continues reading its own fields immediately after this returns.
    static ParseResult<TangibleObjectBaseline6> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. Declared here (not in the .cpp) so
// other schemas composing this type as an ancestor - CreatureObjectBaseline6 -
// can reference it directly.
inline constexpr FieldDescriptor kTangibleObjectBaseline6Fields[] = {
    fieldBaselineOnly<FieldKind::Int32, &TangibleObjectBaseline6::unknownConst>("unknownConst"),
    fieldBaselineOnly<FieldKind::Uint64Container, &TangibleObjectBaseline6::defenders>(
        "defenders"),
};

inline constexpr ObjectSchema kTangibleObjectBaseline6Schema{nullptr, 0,
                                                               kTangibleObjectBaseline6Fields};

} // namespace swgproto
