#pragma once

#include <chrono>
#include <cstdint>

namespace worldmodel {

// Which concrete type is stored for a given objectId - the discriminant of
// ObjectVariant (see ObjectVariant.h). Kept in sync with ObjectVariant's
// alternative list by construction: ObjectStore only ever constructs a
// variant alternative and this tag together, in the same place (see
// ObjectStore.cpp) - never set independently, so they can't drift apart.
enum class ObjectTypeTag : uint8_t {
    Creature,
    Tangible,
    Player,
    Weapon,
    Intangible,
    Installation,
    ResourceContainer,
    FactoryCrate,
    Static,
    Cell,
    Group,
    Unknown,
};

// Common bookkeeping every concrete world-object type inherits. Deliberately
// concrete (no virtual functions): every existing decode primitive in this
// project (FieldDescriptor, ObjectSchema, ParseResult<T>) is non-virtual and
// type-erased via plain function pointers to stay free of vtable overhead
// and object-slicing risk - a virtual WorldObject would be the first
// exception to that established style, and would also re-derive information
// ObjectStateDispatcher's (objectType, baselineNumber) dispatch key already
// hands the caller for free. WorldObject is never stored or passed around as
// a standalone polymorphic handle - it only ever exists as a base subobject
// inside one alternative of ObjectVariant (see ObjectVariant.h and
// ObjectStore.h's storage/polymorphism design notes).
struct WorldObject {
    uint64_t objectId = 0;
    ObjectTypeTag typeTag = ObjectTypeTag::Unknown;
    uint32_t objectTypeFourCC = 0; // raw wire tag, for diagnostics
    bool isSelf = false;           // objectId == the connection's own characterId

    // The template CRC from SceneCreateObjectByCrc (objectCrc), stored by
    // ObjectStore::seedPosition() - see its own comment. Confirmed
    // 2026-07-18 to be SharedObjectTemplate::getClientObjectCRC(), i.e.
    // String::hashCode() of the object's real ".iff" template path
    // server-side (read directly from the Core3 reference source) - the
    // same algorithm already ported as soe::MessageHash::compute(). Zero
    // until the first SceneCreateObjectByCrc for this object seeds it;
    // real templates hash to a real nonzero value in practice, so 0 also
    // works as an implicit "not yet known" sentinel.
    uint32_t objectCrc = 0;

    std::chrono::steady_clock::time_point lastUpdate{};
    uint32_t baselineMessagesSeen = 0;
    uint32_t deltaMessagesSeen = 0;
    uint32_t deltaFieldsApplied = 0; // cumulative, diagnostic
    uint32_t deltaFieldsSkipped = 0; // cumulative "stoppedEarly" or delta-before-baseline count

    // Position/orientation, updated by UpdateTransformMessage (world-relative,
    // parentId left at 0) or UpdateTransformWithParentMessage (cell-relative,
    // real parentId) via ObjectStore::applyTransform()/
    // applyTransformWithParent() - see ObjectStore.cpp. Deliberately a
    // SEPARATE bookkeeping trail from lastUpdate/baselineMessagesSeen/etc.
    // above (those track baseline/delta traffic; this tracks the unrelated
    // raw position-broadcast messages) rather than conflating the two.
    // Zero-valued and transformMessagesSeen==0 until the first transform
    // update ever arrives for this object - many objects (stationary
    // structures/terminals) may never move at all after their baseline, so a
    // zero position must not be read as "this object is at the origin"
    // without checking transformMessagesSeen too.
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    // 0 = world-relative (no parent/cell) - the same "0 means none"
    // convention UpdateContainmentMessage::containerId already uses.
    uint64_t parentId = 0;

    // Logical/inventory-style containment (set by UpdateContainmentMessage
    // via ObjectStore::applyContainment()) - deliberately a SEPARATE field
    // from parentId above, which is spatial/transform-space (set by
    // UpdateTransformWithParentMessage/DataTransformWithParent). The two
    // are different real mechanisms that happen to often agree (e.g. an
    // object inside a building cell usually has containerId == parentId ==
    // the cell's objectId) but aren't guaranteed to - GAP_ANALYSIS.md
    // documents this distinction; don't conflate them. 0 = not contained in
    // anything (top-level world object, or containment never seen yet -
    // check containmentMessagesSeen before treating 0 as authoritative).
    uint64_t containerId = 0;
    uint32_t containmentMessagesSeen = 0;
    uint32_t movementCounter = 0;
    int8_t speed = 0;
    int8_t direction = 0;
    uint32_t transformMessagesSeen = 0;
    std::chrono::steady_clock::time_point lastTransformUpdate{};

    // Real orientation quaternion, updated by DataTransform/
    // DataTransformWithParent (an "idle sync" pushed at zone-in and
    // periodically thereafter for a stationary object - see
    // swgproto/DataTransform.h) via ObjectStore::applyDataTransform()/
    // applyDataTransformWithParent(). Higher-fidelity than `direction`
    // above (UpdateTransformMessage's lossy yaw-only byte) - not currently
    // consumed for pitch/roll by anything (the visualizer still only draws
    // yaw, derived from this same quaternion via the identical formula
    // Core3's own getSpecialDegrees() uses, and written into `direction`
    // too so the existing rendering path needs no changes), but stored in
    // full so a future pass can use it without re-decoding anything.
    // Defaults to the identity quaternion (no rotation) until a real
    // DataTransform arrives - check dataTransformMessagesSeen > 0 before
    // trusting it over the default.
    float quatX = 0.0f;
    float quatY = 0.0f;
    float quatZ = 0.0f;
    float quatW = 1.0f;
    uint32_t dataTransformMessagesSeen = 0;
    std::chrono::steady_clock::time_point lastDataTransformUpdate{};
};

} // namespace worldmodel
