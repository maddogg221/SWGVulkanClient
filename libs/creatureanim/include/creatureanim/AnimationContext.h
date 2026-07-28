#pragma once

#include <cstdint>
#include <string>

#include "creatureanim/CreatureState.h"
#include "worldmodel/CreatureObject.h"

namespace creatureanim {

// Real per-creature context relevant to animation-state selection, resolved
// from a real `worldmodel::CreatureObject`'s current baseline state. Every
// field here is real, server-sent data this project already fully decodes
// (via the same generic baseline/delta schema machinery every other
// baseline field uses - `posture`/`stateBitmask` on BASE3,
// `moodString`/`weaponId`/`performanceAnimation` on BASE6) - none of it
// needed new decode plumbing, only a place to actually read it. Deliberately
// not self-only (unlike the render-side skeletal-mesh pipeline that
// consumes it today) - built from any real `CreatureObject`, so animating
// other visible creatures later doesn't need this struct redesigned.
struct CreatureAnimationContext {
    // CreatureObjectBaseline3::posture - real Core3 enum: 0=upright,
    // 1=crouched, 2=prone, 3=sneaking, 8=sitting, 9=skill-animating.
    uint8_t posture = 0;
    // Locally-derived client-side locomotion intent (real WASD input this
    // frame) - NOT part of CreatureObject itself, so not resolved by
    // `resolveAnimationContext` below; the caller (which already tracks
    // its own movement intent) sets this directly.
    bool isMoving = false;
    // CreatureObjectBaseline3::stateBitmask - see CreatureState.h.
    uint64_t stateBitmask = 0;
    // CreatureObjectBaseline6::moodString - real server-driven mood value,
    // e.g. for the real "mood" Switch axis inside a `.lat` state's own
    // selection tree (see assets::AnimationSelectionContext::mood).
    std::string moodString;
    // CreatureObjectBaseline6::weaponId - real object id of the currently
    // equipped weapon, 0 if none. Not yet resolved to a weapon TYPE/
    // category (would need a real template lookup this project doesn't do
    // yet) - kept as the raw id so callers can at least distinguish
    // "armed" from "unarmed" until that lookup exists.
    uint64_t weaponId = 0;
    // CreatureObjectBaseline6::performanceAnimation - real string, likely a
    // direct `.lat` state name for entertainer performances. Not consumed
    // by state selection yet (no real entertainer-performance state names
    // have been confirmed against real `.lat` data) - carried through for
    // future use and diagnostic visibility.
    std::string performanceAnimation;

    bool inCombat() const { return hasState(stateBitmask, CreatureStateBit::Combat); }
    bool isSwimming() const { return hasState(stateBitmask, CreatureStateBit::Swimming); }
    bool isArmed() const { return weaponId != 0; }
};

// Builds a real animation-selection context from a real CreatureObject's
// CURRENT baseline state (whatever's in `creature.base3`/`creature.base6`
// right now - live-updated via the existing generic delta machinery, see
// ObjectStore::applyDelta). Missing optional baselines (never received, or
// this isn't actually a live-tracked creature) resolve to the struct's own
// defaults rather than throwing - same graceful-degradation convention as
// every other resolver in this project. `isMoving` is caller-supplied (see
// CreatureAnimationContext::isMoving's own comment).
CreatureAnimationContext resolveAnimationContext(const worldmodel::CreatureObject& creature, bool isMoving);

} // namespace creatureanim
