#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/StringId.h"

namespace swgproto {

// IntangibleObject's BASE3 baseline fields - the ancestor layer
// PlayerObjectMessage3 (see PlayerObjectBaseline3.h) extends, the same way
// CreatureObjectMessage3 extends TangibleObjectMessage3. Field order
// confirmed directly from Core3's IntangibleObjectMessage3.h insert*()
// sequence (default opcnt 5, fields 0x00-0x04) - PlayerObjectMessage3's own
// fields continue this same flat index counter from 0x05.
struct IntangibleObjectBaseline3 {
    float complexity = 0.0f;          // 0x00 - always the literal 1 per source, not object state
    StringId objectName;              // 0x01
    std::u16string customObjectName;  // 0x02
    int32_t volume = 0;               // 0x03
    int32_t status = 0;               // 0x04 - source comments it "Unused?"

    // Parses these 5 fields from an already-unwrapped BASE3 payload (the
    // caller has already consumed the shared BaselineEnvelope). Does NOT
    // consume anything beyond these 5 fields - PlayerObjectBaseline3
    // continues reading its own fields immediately after this returns.
    // Implemented via the schema engine (see kIntangibleObjectBaseline3Schema
    // below) - PLAN.md's schema-engine plan, Stage 3.
    static ParseResult<IntangibleObjectBaseline3> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. Declared here (not in the .cpp) so
// PlayerObjectBaseline3, once migrated, can reference it as an ancestor -
// same pattern as TangibleObjectBaseline3.h.
inline constexpr FieldDescriptor kIntangibleObjectBaseline3Fields[] = {
    field<FieldKind::Float, &IntangibleObjectBaseline3::complexity>(0x00, "complexity"),
    field<FieldKind::StringIdField, &IntangibleObjectBaseline3::objectName>(0x01, "objectName"),
    field<FieldKind::Unicode, &IntangibleObjectBaseline3::customObjectName>(0x02,
                                                                             "customObjectName"),
    field<FieldKind::Int32, &IntangibleObjectBaseline3::volume>(0x03, "volume"),
    field<FieldKind::Int32, &IntangibleObjectBaseline3::status>(0x04, "status"),
};

inline constexpr ObjectSchema kIntangibleObjectBaseline3Schema{nullptr, 0,
                                                                 kIntangibleObjectBaseline3Fields};

} // namespace swgproto
