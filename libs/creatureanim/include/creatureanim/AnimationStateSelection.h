#pragma once

#include "assets/AnimationStateTable.h"
#include "creatureanim/AnimationContext.h"

namespace creatureanim {

// Real posture/locomotion -> `.lat` state name map, generalized from this
// project's original self-only, posture+isMoving-only version
// (`Visualizer.cpp`'s own `selfAnimationStateNameFor`, now removed in favor
// of this). Real transition clips (`trn_standing_to_kneeling` etc.,
// confirmed present in the real state table) are deliberately not wired up
// yet - hard cuts between states are fine, per existing project direction;
// swapping to something richer only needs this function (plus playing a
// transition clip first) touched, not a redesign.
//
// `context.stateBitmask`/`weaponId` are real, live-updated signals (see
// CreatureAnimationContext's own comment) but NOT yet used to select a
// different state name here: the one real, confirmed weapon-specific idle
// state name found so far (`loop_pistol_standing`) resolves to a `FORM
// KFAT` clip, a real, different, unimplemented format (see
// PHASE_21_STATUS.md) - selecting it would just silently fail to resolve a
// clip. Left as a real, visible, documented gap rather than guessed at.
const char* stateNameFor(const CreatureAnimationContext& context);

// Builds a real `assets::AnimationSelectionContext` from this context -
// `gender` isn't part of `CreatureAnimationContext` (not derivable from
// `CreatureObject`/baseline data this project has decoded - see
// SkeletalAppearance's own gender resolution), so it's a separate
// parameter here, same as it always was at the call site.
assets::AnimationSelectionContext toSelectionContext(const CreatureAnimationContext& context,
                                                       const std::string& gender);

} // namespace creatureanim
