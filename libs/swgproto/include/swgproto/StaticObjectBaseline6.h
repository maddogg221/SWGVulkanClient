#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// StaticObject's BASE6 baseline (tag "OATS", 0x4F415453). Standalone, no
// delta message class exists (see StaticObjectBaseline3.h). Field order
// confirmed from StaticObjectMessage6.h's insert*() sequence (opCount
// literal = 2, real field count = 4). The second field is a hardcoded
// ASCII literal "String_id_table" (not object-derived) in every real
// instance Core3 sends - modeled as a real field rather than skipped, same
// "document the constant honestly" precedent as ResourceContainer BASE6's
// always-empty fields.
struct StaticObjectBaseline6 {
    int32_t unknownField0 = 0;    // 0x00 - always insertInt(0x44) in source
    std::string literalString;    // 0x01 - always "String_id_table" in source
    int32_t unknownField1 = 0;    // 0x02 - always insertInt(0) in source
    uint16_t unknownField2 = 0;   // 0x03 - always insertShort(0) in source

    static ParseResult<StaticObjectBaseline6> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kStaticObjectBaseline6Fields[] = {
    fieldBaselineOnly<FieldKind::Int32, &StaticObjectBaseline6::unknownField0>("unknownField0"),
    fieldBaselineOnly<FieldKind::Ascii, &StaticObjectBaseline6::literalString>("literalString"),
    fieldBaselineOnly<FieldKind::Int32, &StaticObjectBaseline6::unknownField1>("unknownField1"),
    fieldBaselineOnly<FieldKind::Uint16, &StaticObjectBaseline6::unknownField2>("unknownField2"),
};

inline constexpr ObjectSchema kStaticObjectBaseline6Schema{nullptr, 0,
                                                             kStaticObjectBaseline6Fields};

} // namespace swgproto
