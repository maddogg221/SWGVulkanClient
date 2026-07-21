#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <variant>

#include "swgproto/DataTransform.h"
#include "swgproto/DataTransformWithParent.h"
#include "swgproto/ObjectStateDispatcher.h"
#include "swgproto/UpdateTransformMessage.h"
#include "swgproto/UpdateTransformWithParentMessage.h"
#include "worldmodel/ObjectVariant.h"
#include "worldmodel/WorldObject.h"

namespace worldmodel {

// The central in-memory world-state store: one entry per objectId, combining
// fields from every baseline number received for it into a single record -
// the thing this project didn't have before (every prior decode was
// parse-print-discard, with zero cross-baseline correlation - see PLAN.md's
// Object Model design writeup for the full rationale).
//
// Owns its own registration onto an ObjectStateDispatcher (mirrors
// clientcommon::registerObjControllerHandlers' "give me your dispatcher,
// I'll wire myself onto it" precedent) rather than wrapping/subclassing the
// dispatcher itself - registerHandlers() is a member function, not a free
// function, since (unlike that precedent, which is pure print-and-forget)
// this class owns real state its callbacks mutate.
//
// Thread-safe as of Phase 14 (engine loop redesign): a real network thread
// now writes into this store (via registerHandlers()'s callbacks and the
// applyTransform()-family methods) while the render thread reads from it
// every frame, so every public method that touches objects_/pendingSeeds_
// takes an internal mutex - callers never need their own external locking.
// forEach()'s callback runs WHILE the lock is held (covering the whole
// iteration, not just each individual visit) - it must not call back into
// this same ObjectStore instance, or it will deadlock (a plain
// std::mutex, not recursive, deliberately - re-entrant store access from a
// draw callback was never a real use case worth the extra complexity of a
// recursive lock).
class ObjectStore {
public:
    explicit ObjectStore(uint64_t selfCharacterId) : selfCharacterId_(selfCharacterId) {}

    // Claims onBaseline/onDelta for exactly the (objectType, baselineNumber)
    // keys this pilot migration covers (TANO 3/6, RCNO 3/6 baseline; TANO
    // 3/6, RCNO 3 delta - no RCNO delta 6, matching the fact that
    // ResourceContainerBaseline6's delta setters are confirmed dead code).
    // Registering the same key twice just replaces the handler
    // (ObjectStateDispatcher's own documented behavior), so this is safe to
    // call once against a dispatcher no one else has already claimed these
    // keys on.
    void registerHandlers(swgproto::ObjectStateDispatcher& dispatcher);

    // Applies a real UpdateTransformMessage/UpdateTransformWithParentMessage
    // broadcast onto the matching stored object's shared WorldObject position
    // fields, if one is already tracked (silently a no-op otherwise - a
    // transform broadcast for an objectId this store has never baselined is
    // expected/routine, not a diagnostic-worthy gap, unlike a delta-before-
    // baseline; movement traffic volume is far too high to log every miss).
    // Registered directly by main() on the top-level dispatcher (NOT via
    // ObjectStateDispatcher - these are plain top-level messages, not
    // baseline/delta-enveloped), the same permanent-for-the-connection
    // lifetime as the baseline/delta forwarding and SceneDestroyObject
    // handling. Deliberately the ONLY place these two hashes are ever
    // registered - see the 2026-07-18 movement work in SESSION_LOG.md for why
    // a second, function-local registration (e.g. inside a time-boxed
    // "observe movement" window) would silently clobber this one for its
    // duration and then delete it on the way out, the same bug class already
    // found and fixed twice for kBaselinesMessageHash/kDeltasMessageHash and
    // kSceneDestroyObjectHash.
    void applyTransform(const swgproto::UpdateTransformMessage& msg);
    void applyTransformWithParent(const swgproto::UpdateTransformWithParentMessage& msg);

    // Applies a real UpdateContainmentMessage broadcast onto the matching
    // stored object's containerId (see WorldObject::containerId's own
    // comment for why this is a separate field from parentId, not a
    // duplicate). Same "registered directly by main() on the top-level
    // dispatcher, permanent for the whole connection, silent no-op for an
    // untracked objectId" treatment as applyTransform() above - see its
    // comment; the exact same forwarding-kill bug class (a per-phase
    // register/unregister cycle silently losing this hash's only handler)
    // was found and fixed for this message too during Phase 17, not just
    // inferred by analogy - real capture traffic showed 286 real
    // UpdateContainmentMessage occurrences in one session, most long after
    // the zone-in window the old per-phase handler was scoped to.
    void applyContainment(uint64_t objectId, uint64_t containerId);

    // DataTransform/DataTransformWithParent are ObjControllerMessage
    // sub-types (unlike the two methods above), so `objectId` comes from
    // the shared envelope the caller already parsed, not from the struct
    // itself. Confirmed live 2026-07-18 (see SESSION_LOG.md): a real
    // DataTransform reliably arrives for the self character right at
    // zone-in, even on a fully passive connection that never sends
    // outgoing movement - closing the gap where self (and other stationary
    // objects) never got a real position from applyTransform() above,
    // since nothing they do ever triggers an UpdateTransformMessage
    // broadcast. Updates the SAME position/`transformMessagesSeen` fields
    // applyTransform() does (so the existing "has this object ever gotten
    // a real position" check works uniformly regardless of which message
    // provided it), PLUS the real orientation quaternion fields, PLUS
    // derives and writes a yaw byte into `direction` using the identical
    // formula Core3's own Quaternion::getSpecialDegrees() uses - so the
    // existing (yaw-only) rendering path picks this up with zero changes,
    // while the full quaternion stays available for a future pitch/roll
    // pass. Same "registered directly by main(), silently a no-op for an
    // untracked objectId" treatment as applyTransform() above.
    void applyDataTransform(uint64_t objectId, const swgproto::DataTransform& msg);
    void applyDataTransformWithParent(uint64_t objectId,
                                       const swgproto::DataTransformWithParent& msg);

    // Seeds an object's position+orientation from SceneCreateObjectByCrc -
    // the message Core3 sends for EVERY object as it spawns into view,
    // carrying a real position and orientation quaternion that was
    // previously discarded (only objectId was ever used, for existence
    // tracking - see SceneCreateObjectByCrc.h). Added 2026-07-18 after
    // DataTransform (the originally planned fix for this same gap) was
    // confirmed live to be unreliable for self: Core3's idle-sync only
    // fires once an object's own moveCount crosses 50, which requires real
    // outbound movement dummyclient's passive design never generates - self
    // stayed invisible even after the character was walked around and left
    // idle via a separate live client, then reconnected fresh. This is
    // guaranteed instead of conditional: it is how the server tells the
    // client "here is a new object, at this spot," for every object, every
    // time, closing the gap for self AND every other never-moved stationary
    // object in one pass.
    //
    // Unlike applyTransform()/applyDataTransform() above (silent no-ops for
    // an untracked objectId), SceneCreateObjectByCrc routinely arrives
    // BEFORE an object's own baseline - so an unseen objectId here is
    // buffered (see pendingSeeds_) and applied automatically the moment
    // that objectId's first baseline creates its ObjectStore entry (see
    // applyBaseline()'s own pending-seed check), rather than dropped.
    //
    // `objectCrc` is also stored (see WorldObject::objectCrc's own comment) -
    // added 2026-07-18 alongside the real-mesh-asset pipeline, since this is
    // the exact same message that already carries it and every consumer of
    // one already needs the other.
    void seedPosition(uint64_t objectId, float x, float y, float z, float quatX, float quatY,
                       float quatZ, float quatW, uint32_t objectCrc);

    const WorldObject* find(uint64_t objectId) const;

    // Copies out just the shared WorldObject base fields (position/parentId/
    // containerId/etc.) for every tracked object, in one single locked pass.
    // Added for Phase 17 Step 3 (cell-relative rendering): resolving an
    // object's world position needs to look up OTHER objects (its
    // containing cell, that cell's containing building) - calling find()
    // for those lookups from inside a forEach() callback would re-lock this
    // store's already-held, non-recursive mutex on the same thread, a real
    // deadlock this project hit live (Release build threw "resource
    // deadlock would occur"; Debug build just hung). Callers needing
    // multi-object lookups during a forEach() pass should snapshot once
    // before the pass instead of calling back into this store from inside
    // it - see resolveWorldPosition() in tools/dummyclient/Visualizer.cpp
    // for the real caller this was built for.
    std::unordered_map<uint64_t, WorldObject> snapshotWorldObjects() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_map<uint64_t, WorldObject> snapshot;
        snapshot.reserve(objects_.size());
        for (const auto& [id, variant] : objects_) {
            snapshot.emplace(
                id, std::visit([](const auto& o) -> const WorldObject& { return o; }, variant));
        }
        return snapshot;
    }

    template <typename T>
    const T* findAs(uint64_t objectId) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = objects_.find(objectId);
        if (it == objects_.end()) {
            return nullptr;
        }
        return std::get_if<T>(&it->second);
    }

    // Calls fn(const ConcreteT&) for every stored object via std::visit -
    // the type-agnostic iteration path (e.g. a future renderer listing
    // everything nearby). fn should be a generic lambda (auto& parameter) to
    // handle every alternative in ObjectVariant. Holds the lock for the
    // WHOLE iteration (see the class comment's note on re-entrancy).
    template <typename Fn>
    void forEach(Fn&& fn) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& [id, variant] : objects_) {
            std::visit(fn, variant);
        }
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return objects_.size();
    }

    // No automatic removal happens INSIDE this class - the real despawn
    // signal (SceneDestroyObject, hash 0x4D45D504) is a plain top-level
    // message, not baseline/delta-enveloped, so it's decoded and this
    // method is called directly from main() (the same "plain top-level
    // message, registered directly by the caller" pattern applyTransform()/
    // applyTransformWithParent() above now also use) rather than owned here.
    // See KNOWN_UNKNOWNS.md's "CLOSED 2026-07-18" entry for the full trace.
    void remove(uint64_t objectId) {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.erase(objectId);
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        objects_.clear();
    }

private:
    // Finds-or-inserts objects_[objectId] as a fresh ConcreteT if this is
    // the first baseline ever seen for it, decodes `buf` into the field
    // named by `slot` via the existing swgproto::decodeBaselineInto, and
    // stamps WorldObject's shared bookkeeping fields. Template-deduced
    // entirely from `slot`'s type (e.g. &TangibleObject::base3) - callers
    // never need to spell out ConcreteT/BaselineStructT explicitly.
    template <typename ConcreteT, typename BaselineStructT>
    void applyBaseline(uint64_t objectId, uint32_t objectTypeFourCC, ObjectTypeTag tag,
                        std::optional<BaselineStructT> ConcreteT::*slot,
                        const swgproto::ObjectSchema& schema, soe::PacketBuffer& buf);

    // Applies a delta directly onto the ALREADY-STORED object's `slot`, via
    // swgproto::applyDeltaMessage. Drops (with a diagnostic + counter, never
    // a crash or a guessed default value) if the objectId was never seen at
    // all, or if this specific slot's baseline hasn't arrived yet - the
    // delta-before-baseline race is a real, explicitly-handled case, not an
    // oversight.
    template <typename ConcreteT, typename BaselineStructT>
    void applyDelta(uint64_t objectId, std::optional<BaselineStructT> ConcreteT::*slot,
                     const swgproto::ObjectSchema& schema, uint16_t count, soe::PacketBuffer& buf);

    // GroupObject BASE6's delta bypasses the generic applyDelta<> template
    // above entirely - see GroupObjectDelta6.h's comment for why (a
    // fundamentally different "incrementally mutate an existing list via
    // per-entry tags" contract, not "read one fresh value per index").
    // Mirrors applyDelta<>'s unseen-objectId/no-baseline-yet handling
    // exactly, just calling swgproto::applyGroupObjectBaseline6Delta instead
    // of swgproto::applyDeltaMessage.
    void applyGroupDelta6(uint64_t objectId, uint16_t count, soe::PacketBuffer& buf);

    // HarvesterObject BASE7's delta bypasses applyDelta<> for the same
    // reason as applyGroupDelta6 above (see HarvesterObjectDelta7.h) - and
    // is sent under a DIFFERENT tag ("INSO") than its own baseline
    // ("HINO"), composing onto the same InstallationObject::base7 slot the
    // "HINO",7 baseline handler already populates.
    void applyHarvesterDelta7(uint64_t objectId, uint16_t count, soe::PacketBuffer& buf);

    // A SceneCreateObjectByCrc seen before that objectId's own first
    // baseline - see seedPosition()'s comment. Applied and erased the
    // moment applyBaseline() creates the matching entry.
    struct PendingSeed {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float quatX = 0.0f;
        float quatY = 0.0f;
        float quatZ = 0.0f;
        float quatW = 1.0f;
        uint32_t objectCrc = 0;
    };

    uint64_t selfCharacterId_;
    mutable std::mutex mutex_; // guards objects_/pendingSeeds_ - see class comment
    std::unordered_map<uint64_t, ObjectVariant> objects_;
    std::unordered_map<uint64_t, PendingSeed> pendingSeeds_;
};

} // namespace worldmodel
