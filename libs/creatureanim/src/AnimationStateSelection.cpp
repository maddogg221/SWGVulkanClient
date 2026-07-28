#include "creatureanim/AnimationStateSelection.h"

namespace creatureanim {

const char* stateNameFor(const CreatureAnimationContext& context) {
    if (context.isMoving && context.posture == 0) {
        // "loop_combat_standing" is a real SPAT-variant state whose own
        // first real clip (appearance/animation/all_b_loc_walk_male.ans) is
        // the walk cycle - confirmed via a real dump. Despite the "combat"
        // name, the clip itself is generically named/usable for ordinary
        // walking, real combat state or not.
        return "loop_combat_standing";
    }
    switch (context.posture) {
        case 1: return "loop_kneeling";
        case 2: return "loop_prone";
        case 8: return "loop_sitting_chair";
        // Real Core3 posture 13 (INCAPACITATED, per CreaturePosture.h) has a
        // real, confirmed `.lat` state - "face up" is the more common real
        // knockdown orientation of the two real variants
        // (loop_incapacitated_face_up/_face_down both exist; there's no
        // real per-object signal yet to pick between them).
        case 13: return "loop_incapacitated_face_up";
        // Real posture 14 (DEAD) has NO real corresponding `.lat` state -
        // confirmed by a direct string search of the real state table
        // (every real state name, `all_m.lat`/`all_b.lat` share one file),
        // zero matches for "dead" anywhere. Death is very likely rendered
        // as a corpse/ragdoll through a real, separate mechanism entirely
        // outside the normal animation-state system this project
        // implements - falls through to the same default as every other
        // unmapped posture rather than guessing a state name that doesn't
        // exist. (Found live, 2026-07-28: this project's own long-running
        // admin test character had actually been dead the entire time this
        // was investigated - real, confirmed via a live posture=14 read and
        // a successful revive - but reviving it changed nothing rendered,
        // since this fallback already produced the same clip either way.)
        default: return "loop_standing";
    }
}

assets::AnimationSelectionContext toSelectionContext(const CreatureAnimationContext& context,
                                                       const std::string& gender) {
    assets::AnimationSelectionContext selection;
    selection.gender = gender;
    selection.mood = context.moodString;
    return selection;
}

} // namespace creatureanim
