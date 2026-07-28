#include "creatureanim/AnimationContext.h"

namespace creatureanim {

CreatureAnimationContext resolveAnimationContext(const worldmodel::CreatureObject& creature, bool isMoving) {
    CreatureAnimationContext context;
    context.isMoving = isMoving;
    if (creature.base3.has_value()) {
        context.posture = creature.base3->posture;
        context.stateBitmask = creature.base3->stateBitmask;
    }
    if (creature.base6.has_value()) {
        context.moodString = creature.base6->moodString;
        context.weaponId = creature.base6->weaponId;
        context.performanceAnimation = creature.base6->performanceAnimation;
    }
    return context;
}

} // namespace creatureanim
