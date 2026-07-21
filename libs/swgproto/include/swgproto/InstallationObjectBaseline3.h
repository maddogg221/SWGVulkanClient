#pragma once

#include <cstddef>
#include <cstdint>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/TangibleObjectBaseline3.h"

namespace swgproto {

// InstallationObject's BASE3 baseline (tag "INSO", 0x494E534F, confirmed from
// InstallationObjectMessage3.h/InstallationObjectDeltaMessage3.h). Extends
// TangibleObjectMessage3 (calls its constructor directly, so its own 11
// fields - see TangibleObjectBaseline3.h - are written first, unchanged) and
// appends exactly 3 own fields: activeFlag/"Active Flag" (byte), surplusPower
// /"Energy Store" (float), basePowerRate/"Energy Rate" (float). The
// constructor's own opcnt override (0x05) does NOT match the real total field
// count (11 inherited + 3 own = 14) - a known Core3 quirk (opcnt understating
// the real field count, same class of finding as WeaponObject's opcnt
// overstating one) this project's schema engine is immune to, since it never
// trusts opcnt for parsing, only reads fields positionally in declared order.
//
// Field order confirmed directly from InstallationObjectMessage3.h's
// insert*() sequence (its own large historical field-writing block is
// entirely commented out/dead - only the trailing 3 inserts after the
// TangibleObjectMessage3(...) constructor call are live). Delta index 0x0B
// (activeFlag, "operating") confirmed directly from
// InstallationObjectDeltaMessage3.h - it's the ONLY delta setter that class
// exposes, so surplusPower/basePowerRate have no delta path in source at all
// (not just unobserved - structurally absent), hence fieldBaselineOnly.
//
// Confirmed from source, NOT self-only: InstallationObjectImplementation::
// sendBaselinesTo() sends unconditionally to every observer (no `player ==
// thisPointer` gate), and setActive()'s activeFlag delta update is sent via
// an explicit `broadcastMessages(&messages, true)` call, not a self-only
// `sendMessage()`. No real INSO traffic was observed live this session (the
// test character's starting area has no harvesters/factories/turrets
// nearby) - documented honestly, matching this project's established
// precedent for source-confirmed-but-unobserved paths (e.g. WeaponObject's
// delta traffic).
struct InstallationObjectBaseline3 {
    TangibleObjectBaseline3 tangible; // 0x00-0x0A
    bool activeFlag = false;          // 0x0B - "Active Flag" / "operating"
    float surplusPower = 0.0f;        // 0x0C - "Energy Store"
    float basePowerRate = 0.0f;       // 0x0D - "Energy Rate"

    static ParseResult<InstallationObjectBaseline3> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kInstallationObjectBaseline3OwnFields[] = {
    field<FieldKind::Bool, &InstallationObjectBaseline3::activeFlag>(0x0B, "activeFlag"),
    fieldBaselineOnly<FieldKind::Float, &InstallationObjectBaseline3::surplusPower>(
        "surplusPower"),
    fieldBaselineOnly<FieldKind::Float, &InstallationObjectBaseline3::basePowerRate>(
        "basePowerRate"),
};

inline constexpr ObjectSchema kInstallationObjectBaseline3Schema{
    &kTangibleObjectBaseline3Schema, offsetof(InstallationObjectBaseline3, tangible),
    kInstallationObjectBaseline3OwnFields};

} // namespace swgproto
