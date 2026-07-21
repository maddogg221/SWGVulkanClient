#pragma once

#include <cstdint>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"

namespace swgproto {

// FactoryCrate's BASE6 baseline (tag "FCYT", 0x46435954). Standalone
// (extends BaseLineMessage directly). Unlike almost every other BASE6 this
// project decodes, this one carries ZERO live data - confirmed from source:
// FactoryCrateObjectMessage6.h's constructor calls no getters at all, only
// hardcoded literals (`insertShort(3)`, 8x `insertInt(0)`,
// `insertByte(0)`). No FactoryCrateObjectDeltaMessage6 class exists in
// source at all (confirmed via a repo-wide search), consistent with there
// being nothing here that could ever change. Modeled as a real schema
// anyway (not skipped, unlike e.g. TangibleObject BASE7/8/9's fully-dead
// stub classes) since this genuinely IS sent as real wire traffic by
// FactoryCrateImplementation::sendBaselinesTo() - documented honestly as
// constant, not omitted.
struct FactoryCrateBaseline6 {
    uint16_t constantThree = 0; // 0x00 - always insertShort(3)
    int32_t unknownField0 = 0;  // 0x01 - always 0
    int32_t unknownField1 = 0;  // 0x02 - always 0
    int32_t unknownField2 = 0;  // 0x03 - always 0
    int32_t unknownField3 = 0;  // 0x04 - always 0
    int32_t unknownField4 = 0;  // 0x05 - always 0
    int32_t unknownField5 = 0;  // 0x06 - always 0
    int32_t unknownField6 = 0;  // 0x07 - always 0
    int32_t unknownField7 = 0;  // 0x08 - always 0
    uint8_t unknownByte = 0;    // 0x09 - always 0

    static ParseResult<FactoryCrateBaseline6> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kFactoryCrateBaseline6Fields[] = {
    fieldBaselineOnly<FieldKind::Uint16, &FactoryCrateBaseline6::constantThree>("constantThree"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField0>("unknownField0"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField1>("unknownField1"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField2>("unknownField2"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField3>("unknownField3"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField4>("unknownField4"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField5>("unknownField5"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField6>("unknownField6"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline6::unknownField7>("unknownField7"),
    fieldBaselineOnly<FieldKind::Byte, &FactoryCrateBaseline6::unknownByte>("unknownByte"),
};

inline constexpr ObjectSchema kFactoryCrateBaseline6Schema{nullptr, 0,
                                                             kFactoryCrateBaseline6Fields};

} // namespace swgproto
