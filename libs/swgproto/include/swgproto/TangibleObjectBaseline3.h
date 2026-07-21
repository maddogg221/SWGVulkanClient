#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/StringId.h"

namespace swgproto {

// TangibleObject's BASE3 baseline fields - the ancestor layer
// CreatureObjectMessage3 (see CreatureObjectBaseline3.h) extends. Field
// order confirmed directly from Core3's TangibleObjectMessage3.h insert*()
// sequence, cross-checked against the delta field indices in
// TangibleObjectDeltaMessage3.h/CreatureObjectDeltaMessage3.h (0x00
// complexity through 0x0A objectVisible - the same flat index counter
// CreatureObjectBaseline3's own fields continue from 0x0B).
struct TangibleObjectBaseline3 {
    float complexity = 0.0f;                 // 0x00
    StringId objectName;                     // 0x01
    std::u16string customObjectName;         // 0x02 - the character's display name
    int32_t volume = 0;                      // 0x03
    std::string customizationString;         // 0x04
    std::vector<int32_t> visibleComponents;  // 0x05
    int32_t optionsBitmask = 0;              // 0x06
    int32_t useCount = 0;                    // 0x07
    int32_t conditionDamage = 0;             // 0x08
    int32_t maxCondition = 0;                // 0x09
    bool objectVisible = false;              // 0x0A

    // Parses these 11 fields from an already-unwrapped BASE3 payload (the
    // caller has already consumed the shared BaselineEnvelope). Does NOT
    // consume anything beyond these 11 fields - CreatureObjectBaseline3
    // continues reading its own fields immediately after this returns.
    // Implemented via the schema engine (see kTangibleObjectBaseline3Schema
    // below) - PLAN.md's schema-engine plan, Stage 2.
    static ParseResult<TangibleObjectBaseline3> parse(soe::PacketBuffer& buf);
};

// Schema-engine table for this type. Declared here (not in the .cpp) so
// other schemas composing this type as an ancestor - CreatureObjectBaseline3,
// once migrated - can reference it directly. This table is a direct
// transcription of the field-index comments above; the compiler checks each
// entry's FieldKind against the referenced member's real C++ type (see
// ObjectSchema.h's field<>() static_assert).
// volume/visibleComponents/objectVisible are fieldBaselineOnly, not field<> -
// TangibleObjectDeltaMessage3.h has no setter for indices 0x03/0x05/0x0A, so
// no live delta ever legitimately targets them (confirmed via Core3 source,
// same "missing is safe, wrong is not" reasoning as BASE9's speciesData/
// friendListStub).
inline constexpr FieldDescriptor kTangibleObjectBaseline3Fields[] = {
    field<FieldKind::Float, &TangibleObjectBaseline3::complexity>(0x00, "complexity"),
    field<FieldKind::StringIdField, &TangibleObjectBaseline3::objectName>(0x01, "objectName"),
    field<FieldKind::Unicode, &TangibleObjectBaseline3::customObjectName>(0x02,
                                                                           "customObjectName"),
    fieldBaselineOnly<FieldKind::Int32, &TangibleObjectBaseline3::volume>("volume"),
    field<FieldKind::Ascii, &TangibleObjectBaseline3::customizationString>(
        0x04, "customizationString"),
    fieldBaselineOnly<FieldKind::Int32Container, &TangibleObjectBaseline3::visibleComponents>(
        "visibleComponents"),
    field<FieldKind::Int32, &TangibleObjectBaseline3::optionsBitmask>(0x06, "optionsBitmask"),
    field<FieldKind::Int32, &TangibleObjectBaseline3::useCount>(0x07, "useCount"),
    field<FieldKind::Int32, &TangibleObjectBaseline3::conditionDamage>(0x08, "conditionDamage"),
    field<FieldKind::Int32, &TangibleObjectBaseline3::maxCondition>(0x09, "maxCondition"),
    fieldBaselineOnly<FieldKind::Bool, &TangibleObjectBaseline3::objectVisible>("objectVisible"),
};

inline constexpr ObjectSchema kTangibleObjectBaseline3Schema{nullptr, 0,
                                                               kTangibleObjectBaseline3Fields};

} // namespace swgproto
