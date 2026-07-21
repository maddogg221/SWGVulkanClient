#include "worldmodel/ObjectStore.h"

#include <algorithm>
#include <cmath>
#include <iostream>

#include "swgproto/CellObjectBaseline3.h"
#include "swgproto/CellObjectBaseline6.h"
#include "swgproto/CreatureObjectBaseline1.h"
#include "swgproto/CreatureObjectBaseline3.h"
#include "swgproto/CreatureObjectBaseline4.h"
#include "swgproto/CreatureObjectBaseline6.h"
#include "swgproto/FactoryCrateBaseline3.h"
#include "swgproto/FactoryCrateBaseline6.h"
#include "swgproto/GroupObjectBaseline3.h"
#include "swgproto/GroupObjectBaseline6.h"
#include "swgproto/GroupObjectDelta6.h"
#include "swgproto/HarvesterObjectBaseline7.h"
#include "swgproto/HarvesterObjectDelta7.h"
#include "swgproto/InstallationObjectBaseline3.h"
#include "swgproto/IntangibleObjectBaseline3.h"
#include "swgproto/PlayerObjectBaseline3.h"
#include "swgproto/PlayerObjectBaseline6.h"
#include "swgproto/PlayerObjectBaseline8.h"
#include "swgproto/PlayerObjectBaseline9.h"
#include "swgproto/ResourceContainerBaseline3.h"
#include "swgproto/ResourceContainerBaseline6.h"
#include "swgproto/SchemaEngine.h"
#include "swgproto/StaticObjectBaseline3.h"
#include "swgproto/StaticObjectBaseline6.h"
#include "swgproto/TangibleObjectBaseline3.h"
#include "swgproto/TangibleObjectBaseline6.h"
#include "worldmodel/CellObject.h"
#include "worldmodel/CreatureObject.h"
#include "worldmodel/FactoryCrate.h"
#include "worldmodel/GroupObject.h"
#include "worldmodel/InstallationObject.h"
#include "worldmodel/IntangibleObject.h"
#include "worldmodel/PlayerObject.h"
#include "worldmodel/ResourceContainer.h"
#include "worldmodel/StaticObject.h"
#include "worldmodel/TangibleObject.h"
#include "worldmodel/WeaponObject.h"

namespace worldmodel {

namespace {

// Derives a 0-100-scaled yaw byte from a real orientation quaternion, using
// the EXACT formula Core3's own Quaternion::getSpecialDegrees() uses
// (confirmed by reading engine3's Quaternion.h directly: getRadians()
// extracts a Y-axis-only heading angle via `2 * acos(w)` - sign-corrected
// against the Y component - then getSpecialDegrees() scales radians to
// 0-100 via `(radians / 2pi) * 100`), so a DataTransform/SceneCreateObjectByCrc
// -derived direction byte lands on the identical value Core3's own client
// would show. `acosf`'s domain is [-1,1]; clamped defensively since
// floating-point quaternion components from the wire could drift a hair
// outside that range and produce a NaN otherwise (a defensive addition, not
// present in Core3's own C++ since it doesn't need to worry about untrusted
// wire floats the same way a decoder does).
uint8_t yawByteFromQuaternion(float qw, float qy) {
    float dirW = qw;
    float angle = 0.0f;
    if (qw * qw + qy * qy > 0.0f) {
        if (qw > 0.0f && qy < 0.0f) {
            dirW = -dirW;
        }
        dirW = std::clamp(dirW, -1.0f, 1.0f);
        angle = 2.0f * std::acos(dirW);
    }
    return static_cast<uint8_t>((angle / 6.283f) * 100.0f);
}

} // namespace

template <typename ConcreteT, typename BaselineStructT>
void ObjectStore::applyBaseline(uint64_t objectId, uint32_t objectTypeFourCC, ObjectTypeTag tag,
                                 std::optional<BaselineStructT> ConcreteT::*slot,
                                 const swgproto::ObjectSchema& schema, soe::PacketBuffer& buf) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        it = objects_.emplace(objectId, ObjectVariant{std::in_place_type<ConcreteT>}).first;
    }

    ConcreteT* concrete = std::get_if<ConcreteT>(&it->second);
    if (concrete == nullptr) {
        // objectId was previously stored as a different concrete type - not
        // expected in practice (an objectId's type shouldn't change
        // mid-session), but handled explicitly rather than silently writing
        // into the wrong storage: replace with a fresh instance of the type
        // this baseline actually claims to be.
        std::cerr << "ObjectStore: objectId=" << objectId
                   << " previously stored as a different type; replacing\n";
        it->second = ObjectVariant{std::in_place_type<ConcreteT>};
        concrete = std::get_if<ConcreteT>(&it->second);
    }

    auto& slotValue = (concrete->*slot).emplace();
    std::string error;
    if (!swgproto::decodeBaselineInto(schema, &slotValue, buf, error)) {
        std::cerr << "ObjectStore: failed to decode baseline for objectId=" << objectId << ": "
                   << error << "\n";
        (concrete->*slot).reset();
        return;
    }

    concrete->objectId = objectId;
    concrete->typeTag = tag;
    concrete->objectTypeFourCC = objectTypeFourCC;
    concrete->isSelf = (objectId == selfCharacterId_);
    concrete->lastUpdate = std::chrono::steady_clock::now();
    ++concrete->baselineMessagesSeen;

    // Applies a SceneCreateObjectByCrc seen for this objectId before its own
    // baseline arrived - see seedPosition()'s comment for why this ordering
    // is the common case, not an edge case.
    auto seedIt = pendingSeeds_.find(objectId);
    if (seedIt != pendingSeeds_.end()) {
        const PendingSeed& seed = seedIt->second;
        concrete->x = seed.x;
        concrete->y = seed.y;
        concrete->z = seed.z;
        concrete->quatX = seed.quatX;
        concrete->quatY = seed.quatY;
        concrete->quatZ = seed.quatZ;
        concrete->quatW = seed.quatW;
        concrete->objectCrc = seed.objectCrc;
        concrete->direction = static_cast<int8_t>(yawByteFromQuaternion(seed.quatW, seed.quatY));
        ++concrete->transformMessagesSeen;
        ++concrete->dataTransformMessagesSeen;
        concrete->lastTransformUpdate = std::chrono::steady_clock::now();
        concrete->lastDataTransformUpdate = concrete->lastTransformUpdate;
        pendingSeeds_.erase(seedIt);
    }
}

template <typename ConcreteT, typename BaselineStructT>
void ObjectStore::applyDelta(uint64_t objectId, std::optional<BaselineStructT> ConcreteT::*slot,
                              const swgproto::ObjectSchema& schema, uint16_t count,
                              soe::PacketBuffer& buf) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        std::cerr << "ObjectStore: delta for unseen objectId=" << objectId
                   << " (no baseline yet), dropping\n";
        return;
    }

    ConcreteT* concrete = std::get_if<ConcreteT>(&it->second);
    if (concrete == nullptr) {
        std::cerr << "ObjectStore: delta for objectId=" << objectId
                   << " targets a different stored type, dropping\n";
        return;
    }

    auto& slotOpt = concrete->*slot;
    if (!slotOpt.has_value()) {
        std::cerr << "ObjectStore: delta for objectId=" << objectId
                   << " arrived before its own baseline, dropping\n";
        ++concrete->deltaFieldsSkipped;
        return;
    }

    auto result = swgproto::applyDeltaMessage(schema, &slotOpt.value(), count, buf);
    concrete->deltaFieldsApplied += static_cast<uint32_t>(result.appliedFieldIndices.size());
    if (result.stoppedEarly) {
        ++concrete->deltaFieldsSkipped;
    }
    concrete->lastUpdate = std::chrono::steady_clock::now();
    ++concrete->deltaMessagesSeen;
}

void ObjectStore::applyGroupDelta6(uint64_t objectId, uint16_t count, soe::PacketBuffer& buf) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        std::cerr << "ObjectStore: delta for unseen objectId=" << objectId
                   << " (no baseline yet), dropping\n";
        return;
    }

    GroupObject* concrete = std::get_if<GroupObject>(&it->second);
    if (concrete == nullptr) {
        std::cerr << "ObjectStore: delta for objectId=" << objectId
                   << " targets a different stored type, dropping\n";
        return;
    }

    if (!concrete->base6.has_value()) {
        std::cerr << "ObjectStore: delta for objectId=" << objectId
                   << " arrived before its own baseline, dropping\n";
        ++concrete->deltaFieldsSkipped;
        return;
    }

    auto result = swgproto::applyGroupObjectBaseline6Delta(concrete->base6.value(), count, buf);
    concrete->deltaFieldsApplied += static_cast<uint32_t>(result.appliedFieldIndices.size());
    if (result.stoppedEarly) {
        ++concrete->deltaFieldsSkipped;
    }
    concrete->lastUpdate = std::chrono::steady_clock::now();
    ++concrete->deltaMessagesSeen;
}

void ObjectStore::applyHarvesterDelta7(uint64_t objectId, uint16_t count, soe::PacketBuffer& buf) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        std::cerr << "ObjectStore: delta for unseen objectId=" << objectId
                   << " (no baseline yet), dropping\n";
        return;
    }

    InstallationObject* concrete = std::get_if<InstallationObject>(&it->second);
    if (concrete == nullptr) {
        std::cerr << "ObjectStore: delta for objectId=" << objectId
                   << " targets a different stored type, dropping\n";
        return;
    }

    if (!concrete->base7.has_value()) {
        std::cerr << "ObjectStore: delta for objectId=" << objectId
                   << " arrived before its own BASE7 baseline, dropping\n";
        ++concrete->deltaFieldsSkipped;
        return;
    }

    auto result = swgproto::applyHarvesterObjectBaseline7Delta(concrete->base7.value(), count, buf);
    concrete->deltaFieldsApplied += static_cast<uint32_t>(result.appliedFieldIndices.size());
    if (result.stoppedEarly) {
        ++concrete->deltaFieldsSkipped;
    }
    concrete->lastUpdate = std::chrono::steady_clock::now();
    ++concrete->deltaMessagesSeen;
}

void ObjectStore::registerHandlers(swgproto::ObjectStateDispatcher& dispatcher) {
    dispatcher.onBaseline(
        "TANO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Tangible,
                          &TangibleObject::base3, swgproto::kTangibleObjectBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "TANO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Tangible,
                          &TangibleObject::base6, swgproto::kTangibleObjectBaseline6Schema, buf);
        });
    dispatcher.onDelta(
        "TANO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &TangibleObject::base3,
                       swgproto::kTangibleObjectBaseline3Schema, env.count, buf);
        });
    dispatcher.onDelta(
        "TANO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &TangibleObject::base6,
                       swgproto::kTangibleObjectBaseline6Schema, env.count, buf);
        });

    dispatcher.onBaseline(
        "RCNO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::ResourceContainer,
                          &ResourceContainer::base3, swgproto::kResourceContainerBaseline3Schema,
                          buf);
        });
    dispatcher.onBaseline(
        "RCNO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::ResourceContainer,
                          &ResourceContainer::base6, swgproto::kResourceContainerBaseline6Schema,
                          buf);
        });
    dispatcher.onDelta(
        "RCNO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &ResourceContainer::base3,
                       swgproto::kResourceContainerBaseline3Schema, env.count, buf);
        });
    // No RCNO delta 6 registration - ResourceContainerBaseline6's delta
    // setters are confirmed dead code (see ResourceContainerBaseline6.h),
    // matching today's dummyclient behavior of not registering one either.

    // CreatureObject (tag "CREO"), BASE1/3/4/6. Not self-only-gated here
    // (unlike the old inline handlers this replaces) - BASE1/3/4's baseline
    // is only ever sent by the server for the player's own character
    // (source-confirmed sendBaselinesTo() gates), so there is nothing else
    // for a non-gated store handler to ever receive; gating would be
    // redundant defense, not load-bearing correctness. BASE6 was never
    // self-only in the first place.
    dispatcher.onBaseline(
        "CREO", 1, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Creature,
                          &CreatureObject::base1, swgproto::kCreatureObjectBaseline1Schema, buf);
        });
    dispatcher.onBaseline(
        "CREO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Creature,
                          &CreatureObject::base3, swgproto::kCreatureObjectBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "CREO", 4, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Creature,
                          &CreatureObject::base4, swgproto::kCreatureObjectBaseline4Schema, buf);
        });
    dispatcher.onBaseline(
        "CREO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Creature,
                          &CreatureObject::base6, swgproto::kCreatureObjectBaseline6Schema, buf);
        });
    dispatcher.onDelta(
        "CREO", 1, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &CreatureObject::base1,
                       swgproto::kCreatureObjectBaseline1Schema, env.count, buf);
        });
    dispatcher.onDelta(
        "CREO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &CreatureObject::base3,
                       swgproto::kCreatureObjectBaseline3Schema, env.count, buf);
        });
    dispatcher.onDelta(
        "CREO", 4, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &CreatureObject::base4,
                       swgproto::kCreatureObjectBaseline4Schema, env.count, buf);
        });
    dispatcher.onDelta(
        "CREO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &CreatureObject::base6,
                       swgproto::kCreatureObjectBaseline6Schema, env.count, buf);
        });

    // PlayerObject (tag "PLAY"), BASE3/6/8/9 - see PlayerObject.h's comment
    // on why this is safe to store ungated despite being self-only in
    // practice.
    dispatcher.onBaseline(
        "PLAY", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Player,
                          &PlayerObject::base3, swgproto::kPlayerObjectBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "PLAY", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Player,
                          &PlayerObject::base6, swgproto::kPlayerObjectBaseline6Schema, buf);
        });
    dispatcher.onBaseline(
        "PLAY", 8, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Player,
                          &PlayerObject::base8, swgproto::kPlayerObjectBaseline8Schema, buf);
        });
    dispatcher.onBaseline(
        "PLAY", 9, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Player,
                          &PlayerObject::base9, swgproto::kPlayerObjectBaseline9Schema, buf);
        });
    dispatcher.onDelta(
        "PLAY", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &PlayerObject::base3, swgproto::kPlayerObjectBaseline3Schema,
                       env.count, buf);
        });
    dispatcher.onDelta(
        "PLAY", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &PlayerObject::base6, swgproto::kPlayerObjectBaseline6Schema,
                       env.count, buf);
        });
    dispatcher.onDelta(
        "PLAY", 8, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &PlayerObject::base8, swgproto::kPlayerObjectBaseline8Schema,
                       env.count, buf);
        });
    dispatcher.onDelta(
        "PLAY", 9, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &PlayerObject::base9, swgproto::kPlayerObjectBaseline9Schema,
                       env.count, buf);
        });

    // WeaponObject (tag "WEAO") - reuses TangibleObjectBaseline3/6's schema
    // unchanged (own distinct Object Model type per the resolved design
    // decision - see WeaponObject.h).
    dispatcher.onBaseline(
        "WEAO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Weapon,
                          &WeaponObject::base3, swgproto::kTangibleObjectBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "WEAO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Weapon,
                          &WeaponObject::base6, swgproto::kTangibleObjectBaseline6Schema, buf);
        });
    dispatcher.onDelta(
        "WEAO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &WeaponObject::base3, swgproto::kTangibleObjectBaseline3Schema,
                       env.count, buf);
        });
    dispatcher.onDelta(
        "WEAO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &WeaponObject::base6, swgproto::kTangibleObjectBaseline6Schema,
                       env.count, buf);
        });

    // IntangibleObject (tag "ONTI") - only BASE3 is decoded in swgproto.
    dispatcher.onBaseline(
        "ONTI", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Intangible,
                          &IntangibleObject::base3, swgproto::kIntangibleObjectBaseline3Schema,
                          buf);
        });
    dispatcher.onDelta(
        "ONTI", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &IntangibleObject::base3,
                       swgproto::kIntangibleObjectBaseline3Schema, env.count, buf);
        });

    // InstallationObject (tag "INSO") - BASE6 reuses TangibleObjectBaseline6
    // unchanged (see InstallationObject.h).
    dispatcher.onBaseline(
        "INSO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Installation,
                          &InstallationObject::base3, swgproto::kInstallationObjectBaseline3Schema,
                          buf);
        });
    dispatcher.onBaseline(
        "INSO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Installation,
                          &InstallationObject::base6, swgproto::kTangibleObjectBaseline6Schema,
                          buf);
        });
    dispatcher.onDelta(
        "INSO", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &InstallationObject::base3,
                       swgproto::kInstallationObjectBaseline3Schema, env.count, buf);
        });
    dispatcher.onDelta(
        "INSO", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &InstallationObject::base6,
                       swgproto::kTangibleObjectBaseline6Schema, env.count, buf);
        });

    // HarvesterObject BASE7 (tag "HINO") - composes onto the SAME
    // InstallationObject slot its INSO 3/6 baselines above already populate
    // (see InstallationObject.h's `base7` comment for why). No delta
    // registration: no real BASE7 delta traffic has been captured yet
    // (activation requires a working power grid, not yet set up in this
    // project) - see HarvesterObjectBaseline7.h's class comment.
    dispatcher.onBaseline(
        "HINO", 7, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Installation,
                          &InstallationObject::base7, swgproto::kHarvesterObjectBaseline7Schema,
                          buf);
        });
    // Real BASE7 delta traffic is sent under tag "INSO" (not "HINO") - a
    // confirmed, source-documented tag mismatch (HarvesterObject reuses
    // InstallationObjectDeltaMessage7 unmodified) now also independently
    // confirmed live (two real captures, see HarvesterObjectDelta7.h).
    dispatcher.onDelta(
        "INSO", 7, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyHarvesterDelta7(env.objectId, env.count, buf);
        });

    // CellObject (tag "SCLT") - no delta message class exists for BASE6 at
    // all in source, so no delta registration for it here either.
    dispatcher.onBaseline(
        "SCLT", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Cell, &CellObject::base3,
                          swgproto::kCellObjectBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "SCLT", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Cell, &CellObject::base6,
                          swgproto::kCellObjectBaseline6Schema, buf);
        });
    dispatcher.onDelta(
        "SCLT", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &CellObject::base3, swgproto::kCellObjectBaseline3Schema,
                       env.count, buf);
        });

    // FactoryCrate (tag "FCYT") - no delta message class exists for BASE6.
    dispatcher.onBaseline(
        "FCYT", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::FactoryCrate,
                          &FactoryCrate::base3, swgproto::kFactoryCrateBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "FCYT", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::FactoryCrate,
                          &FactoryCrate::base6, swgproto::kFactoryCrateBaseline6Schema, buf);
        });
    dispatcher.onDelta(
        "FCYT", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyDelta(env.objectId, &FactoryCrate::base3, swgproto::kFactoryCrateBaseline3Schema,
                       env.count, buf);
        });

    // StaticObject (tag "OATS") - no delta message class exists for either
    // baseline number at all, so no delta registrations here.
    dispatcher.onBaseline(
        "OATS", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Static, &StaticObject::base3,
                          swgproto::kStaticObjectBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "OATS", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Static, &StaticObject::base6,
                          swgproto::kStaticObjectBaseline6Schema, buf);
        });

    // GroupObject (tag "GRUP") - genuinely invisible to non-members (see
    // GroupObject.h / DISCOVERY.txt's research entry): baselines are sent
    // individually to each member at join time, never to a bystander, so
    // there is no self-only gating to replicate here beyond what the server
    // already enforces by simply never sending it to anyone else. BASE3 has
    // no delta at all (no GroupObjectDeltaMessage3 class exists in source).
    // BASE6's delta bypasses applyDelta<> entirely - see applyGroupDelta6.
    dispatcher.onBaseline(
        "GRUP", 3, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Group, &GroupObject::base3,
                          swgproto::kGroupObjectBaseline3Schema, buf);
        });
    dispatcher.onBaseline(
        "GRUP", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyBaseline(env.objectId, env.objectType, ObjectTypeTag::Group, &GroupObject::base6,
                          swgproto::kGroupObjectBaseline6Schema, buf);
        });
    dispatcher.onDelta(
        "GRUP", 6, [this](const swgproto::BaselineEnvelope& env, soe::PacketBuffer& buf) {
            applyGroupDelta6(env.objectId, env.count, buf);
        });
}

void ObjectStore::applyTransform(const swgproto::UpdateTransformMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(msg.objectId);
    if (it == objects_.end()) {
        return;
    }
    WorldObject& obj = std::visit([](auto& o) -> WorldObject& { return o; }, it->second);
    obj.x = msg.x;
    obj.y = msg.y;
    obj.z = msg.z;
    obj.parentId = 0;
    obj.movementCounter = msg.movementCounter;
    obj.speed = msg.speed;
    obj.direction = msg.direction;
    ++obj.transformMessagesSeen;
    obj.lastTransformUpdate = std::chrono::steady_clock::now();
}

void ObjectStore::applyTransformWithParent(const swgproto::UpdateTransformWithParentMessage& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(msg.objectId);
    if (it == objects_.end()) {
        return;
    }
    WorldObject& obj = std::visit([](auto& o) -> WorldObject& { return o; }, it->second);
    obj.x = msg.x;
    obj.y = msg.y;
    obj.z = msg.z;
    obj.parentId = msg.parentId;
    obj.movementCounter = msg.movementCounter;
    obj.speed = msg.speed;
    obj.direction = msg.direction;
    ++obj.transformMessagesSeen;
    obj.lastTransformUpdate = std::chrono::steady_clock::now();
}

void ObjectStore::applyContainment(uint64_t objectId, uint64_t containerId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        return;
    }
    WorldObject& obj = std::visit([](auto& o) -> WorldObject& { return o; }, it->second);
    obj.containerId = containerId;
    ++obj.containmentMessagesSeen;
}

void ObjectStore::applyDataTransform(uint64_t objectId, const swgproto::DataTransform& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        return;
    }
    WorldObject& obj = std::visit([](auto& o) -> WorldObject& { return o; }, it->second);
    obj.x = msg.x;
    obj.y = msg.y;
    obj.z = msg.z;
    obj.parentId = 0;
    obj.speed = static_cast<int8_t>(msg.speed);
    obj.direction = static_cast<int8_t>(yawByteFromQuaternion(msg.directionW, msg.directionY));
    ++obj.transformMessagesSeen;
    obj.lastTransformUpdate = std::chrono::steady_clock::now();

    obj.quatX = msg.directionX;
    obj.quatY = msg.directionY;
    obj.quatZ = msg.directionZ;
    obj.quatW = msg.directionW;
    ++obj.dataTransformMessagesSeen;
    obj.lastDataTransformUpdate = std::chrono::steady_clock::now();
}

void ObjectStore::applyDataTransformWithParent(uint64_t objectId,
                                                const swgproto::DataTransformWithParent& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        return;
    }
    WorldObject& obj = std::visit([](auto& o) -> WorldObject& { return o; }, it->second);
    obj.x = msg.x;
    obj.y = msg.y;
    obj.z = msg.z;
    obj.parentId = msg.parentId;
    obj.speed = static_cast<int8_t>(msg.speed);
    obj.direction = static_cast<int8_t>(yawByteFromQuaternion(msg.directionW, msg.directionY));
    ++obj.transformMessagesSeen;
    obj.lastTransformUpdate = std::chrono::steady_clock::now();

    obj.quatX = msg.directionX;
    obj.quatY = msg.directionY;
    obj.quatZ = msg.directionZ;
    obj.quatW = msg.directionW;
    ++obj.dataTransformMessagesSeen;
    obj.lastDataTransformUpdate = std::chrono::steady_clock::now();
}

void ObjectStore::seedPosition(uint64_t objectId, float x, float y, float z, float quatX,
                                float quatY, float quatZ, float quatW, uint32_t objectCrc) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        // Baseline hasn't arrived yet - buffer it. applyBaseline() applies
        // and erases this the moment the entry is created.
        pendingSeeds_[objectId] = PendingSeed{x, y, z, quatX, quatY, quatZ, quatW, objectCrc};
        return;
    }
    WorldObject& obj = std::visit([](auto& o) -> WorldObject& { return o; }, it->second);
    obj.x = x;
    obj.y = y;
    obj.z = z;
    obj.direction = static_cast<int8_t>(yawByteFromQuaternion(quatW, quatY));
    obj.objectCrc = objectCrc;
    ++obj.transformMessagesSeen;
    obj.lastTransformUpdate = std::chrono::steady_clock::now();

    obj.quatX = quatX;
    obj.quatY = quatY;
    obj.quatZ = quatZ;
    obj.quatW = quatW;
    ++obj.dataTransformMessagesSeen;
    obj.lastDataTransformUpdate = std::chrono::steady_clock::now();
}

const WorldObject* ObjectStore::find(uint64_t objectId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = objects_.find(objectId);
    if (it == objects_.end()) {
        return nullptr;
    }
    return std::visit([](const auto& obj) -> const WorldObject* { return &obj; }, it->second);
}

} // namespace worldmodel
