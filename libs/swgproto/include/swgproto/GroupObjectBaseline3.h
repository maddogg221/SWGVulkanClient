#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// GroupObject's BASE3 baseline (tag "GRUP", 0x47525550). Standalone, no
// ancestor - extends SceneObject directly (GroupObject.idl).
//
// Every field is a hardcoded placeholder/constant in `GroupObjectMessage3.h`
// - no getters called at all, confirmed both from source and now from a real
// live capture (39/39 bytes, zero leftover, every field matching exactly):
// float(1.0), ascii("string_id_table"), int(0), short(0), int(0), int(0),
// float(1.0). No member/ship list exists in BASE3 at all - that only lives
// in BASE6 (see GroupObjectBaseline6.h). No GroupObjectDeltaMessage3 class
// exists in source at all, so this has zero delta fields - every field here
// is fieldBaselineOnly.
struct GroupObjectBaseline3 {
    float unknownComplexity1 = 0.0f; // always 1.0
    std::string stringIdTable;       // always "string_id_table"
    int32_t unknownField1 = 0;       // always 0
    uint16_t unknownField2 = 0;      // always 0
    int32_t unknownField3 = 0;       // always 0
    int32_t unknownField4 = 0;       // always 0
    float unknownComplexity2 = 0.0f; // always 1.0

    static ParseResult<GroupObjectBaseline3> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kGroupObjectBaseline3Fields[] = {
    fieldBaselineOnly<FieldKind::Float, &GroupObjectBaseline3::unknownComplexity1>(
        "unknownComplexity1"),
    fieldBaselineOnly<FieldKind::Ascii, &GroupObjectBaseline3::stringIdTable>("stringIdTable"),
    fieldBaselineOnly<FieldKind::Int32, &GroupObjectBaseline3::unknownField1>("unknownField1"),
    fieldBaselineOnly<FieldKind::Uint16, &GroupObjectBaseline3::unknownField2>("unknownField2"),
    fieldBaselineOnly<FieldKind::Int32, &GroupObjectBaseline3::unknownField3>("unknownField3"),
    fieldBaselineOnly<FieldKind::Int32, &GroupObjectBaseline3::unknownField4>("unknownField4"),
    fieldBaselineOnly<FieldKind::Float, &GroupObjectBaseline3::unknownComplexity2>(
        "unknownComplexity2"),
};

inline constexpr ObjectSchema kGroupObjectBaseline3Schema{nullptr, 0, kGroupObjectBaseline3Fields};

} // namespace swgproto
