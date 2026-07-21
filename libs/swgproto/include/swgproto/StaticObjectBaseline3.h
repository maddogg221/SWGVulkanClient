#pragma once

#include <cstdint>
#include <string>

#include "soe/PacketBuffer.h"
#include "swgproto/FieldKind.h"
#include "swgproto/ObjectSchema.h"
#include "swgproto/ParseResult.h"
#include "swgproto/StringId.h"

namespace swgproto {

// StaticObject's BASE3 baseline (tag "OATS", 0x4F415453) - world décor:
// campfires, benches, signs, etc. Standalone (extends BaseLineMessage
// directly), and unlike every OTHER "standalone" schema this project has
// built so far, this isn't just "doesn't compose Tangible's constructor" -
// StaticObject genuinely doesn't extend TangibleObject at all in Core3's
// class hierarchy (`StaticObject.idl`: `class StaticObject extends
// SceneObject`), confirmed from source. That also means admin-spawning one
// needs a DIFFERENT command than every other item this project has spawned
// so far: `/object createitem` fails for StaticObject templates (it
// dynamic_casts the created object to TangibleObject*, which fails since
// StaticObject and TangibleObject are unrelated SceneObject subclasses) -
// the correct command is `/createSpawningElement spawn <templatePath>`,
// which spawns directly into the world at the caller's position rather than
// into inventory.
//
// Field order confirmed directly from StaticObjectMessage3.h's insert*()
// sequence (opCount literal = 4, matching the 4 real inserts here - for
// once, no mismatch). No delta message class exists for BASE3 or BASE6 at
// all (confirmed via a repo-wide search) - every field is set once at
// construction from SceneObject-level state or is a hardcoded constant, so
// there is no live delta path for anything here, ever.
//
// Confirmed NOT self-only: StaticObjectImplementation::sendBaselinesTo()
// sends unconditionally to any observer, invoked from the generic
// SceneObjectImplementation::sendTo() broadcast path any nearby player goes
// through.
struct StaticObjectBaseline3 {
    int32_t unknownField0 = 0;        // 0x00 - always insertInt(0) in source
    StringId objectName;              // 0x01
    std::u16string customObjectName;  // 0x02
    int32_t unknownField1 = 0;        // 0x03 - always insertInt(0xFF) in source

    static ParseResult<StaticObjectBaseline3> parse(soe::PacketBuffer& buf);
};

inline constexpr FieldDescriptor kStaticObjectBaseline3Fields[] = {
    fieldBaselineOnly<FieldKind::Int32, &StaticObjectBaseline3::unknownField0>("unknownField0"),
    fieldBaselineOnly<FieldKind::StringIdField, &StaticObjectBaseline3::objectName>("objectName"),
    fieldBaselineOnly<FieldKind::Unicode, &StaticObjectBaseline3::customObjectName>(
        "customObjectName"),
    fieldBaselineOnly<FieldKind::Int32, &StaticObjectBaseline3::unknownField1>("unknownField1"),
};

inline constexpr ObjectSchema kStaticObjectBaseline3Schema{nullptr, 0,
                                                             kStaticObjectBaseline3Fields};

} // namespace swgproto
