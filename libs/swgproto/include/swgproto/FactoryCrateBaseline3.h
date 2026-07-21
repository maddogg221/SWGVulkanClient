#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/StringId.h"

namespace swgproto {

// FactoryCrate's BASE3 baseline (tag "FCYT", 0x46435954) - a stack of
// identical crafted items. Standalone: FactoryCrateObjectMessage3.h extends
// BaseLineMessage directly, NOT TangibleObjectMessage3, despite having a
// TangibleObject-shaped field list (confirmed from source - this is a
// genuinely different class from every other Tangible-composing schema this
// project has built). Constructor's own opCount literal (0x0B=11) doesn't
// match the real field count (12) - same class of Core3 quirk as every
// prior item.
//
// Field order confirmed directly from FactoryCrateObjectMessage3.h's
// insert*() sequence. The first field is a hardcoded literal
// `insertFloat(1.0)` with no getter at all (never `crate->getComplexity()`
// or similar) - modeled as a real field since it's still on the wire, but
// always 1.0 in practice, not driven by object state.
//
// `useCount`/"item count" is the ONLY confirmed delta-addressable field:
// FactoryCrateObjectDeltaMessage3.h's setQuantity(), delta index 0x07 (a
// literal, NOT this field's position in the struct - same
// position-independent-index rule as every other schema here). Confirmed
// genuinely live (not dead code): setQuantity() is called from
// FactoryCrateImplementation::setUseCount(), itself reachable from
// extractObject()/split()/the factory production loop/
// GenerateCraftedItemCommand/and even admin `/object createitem`'s own
// `setUseCount(quantity)` call - multiply-reachable real gameplay code,
// unlike ResourceContainer BASE6's confirmed-dead delta setters.
//
// Confirmed NOT self-only: FactoryCrateImplementation::sendBaselinesTo()
// (FactoryCrate has its own override, not inherited from TangibleObject)
// sends unconditionally to any observer, no `player == thisPointer` gate.
struct FactoryCrateBaseline3 {
    float constantOne = 0.0f;         // 0x00 - always insertFloat(1.0), no real getter
    StringId objectName;              // 0x01
    std::u16string customObjectName;  // 0x02
    int32_t volume = 0;               // 0x03
    std::string customizationString;  // 0x04
    int32_t unknownField0 = 0;        // 0x05 - always insertInt(0) in source
    int32_t unknownField1 = 0;        // 0x06 - always insertInt(0) in source
    int32_t optionsBitmask = 0;       // 0x07
    int32_t useCount = 0;             // 0x08 - "item count"/Stack Size; delta index 0x07 (literal)
    int32_t conditionDamage = 0;      // 0x09
    int32_t maxCondition = 0;         // 0x0A
    bool objectVisible = false;       // 0x0B

    static ParseResult<FactoryCrateBaseline3> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kFactoryCrateBaseline3Fields[] = {
    fieldBaselineOnly<FieldKind::Float, &FactoryCrateBaseline3::constantOne>("constantOne"),
    fieldBaselineOnly<FieldKind::StringIdField, &FactoryCrateBaseline3::objectName>("objectName"),
    fieldBaselineOnly<FieldKind::Unicode, &FactoryCrateBaseline3::customObjectName>(
        "customObjectName"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline3::volume>("volume"),
    fieldBaselineOnly<FieldKind::Ascii, &FactoryCrateBaseline3::customizationString>(
        "customizationString"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline3::unknownField0>("unknownField0"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline3::unknownField1>("unknownField1"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline3::optionsBitmask>("optionsBitmask"),
    field<FieldKind::Int32, &FactoryCrateBaseline3::useCount>(0x07, "useCount"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline3::conditionDamage>(
        "conditionDamage"),
    fieldBaselineOnly<FieldKind::Int32, &FactoryCrateBaseline3::maxCondition>("maxCondition"),
    fieldBaselineOnly<FieldKind::Bool, &FactoryCrateBaseline3::objectVisible>("objectVisible"),
};

inline constexpr ObjectSchema kFactoryCrateBaseline3Schema{nullptr, 0,
                                                             kFactoryCrateBaseline3Fields};

} // namespace swgproto
