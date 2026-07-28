// Permanent regression test for creatureanim's animation-context resolution
// and state-name selection - generalized from Visualizer.cpp's own original
// self-only, posture+isMoving-only version (see PHASE_21_STATUS.md /
// project_visualizer_diagnostic_split memory for the full history).
#include <doctest/doctest.h>

#include "creatureanim/AnimationStateSelection.h"
#include "swgproto/CreatureObjectBaseline3.h"
#include "swgproto/CreatureObjectBaseline6.h"
#include "worldmodel/CreatureObject.h"

using creatureanim::CreatureAnimationContext;
using creatureanim::resolveAnimationContext;
using creatureanim::stateNameFor;
using creatureanim::toSelectionContext;
using worldmodel::CreatureObject;

TEST_CASE("creatureanim::stateNameFor - real posture/locomotion mapping") {
    CreatureAnimationContext ctx;

    ctx.posture = 0;
    ctx.isMoving = false;
    CHECK(std::string(stateNameFor(ctx)) == "loop_standing");

    ctx.isMoving = true;
    CHECK(std::string(stateNameFor(ctx)) == "loop_combat_standing");

    ctx.isMoving = false;
    ctx.posture = 1;
    CHECK(std::string(stateNameFor(ctx)) == "loop_kneeling");

    ctx.posture = 2;
    CHECK(std::string(stateNameFor(ctx)) == "loop_prone");

    ctx.posture = 8;
    CHECK(std::string(stateNameFor(ctx)) == "loop_sitting_chair");

    // Real posture 13 (INCAPACITATED) has a real confirmed .lat state.
    ctx.posture = 13;
    CHECK(std::string(stateNameFor(ctx)) == "loop_incapacitated_face_up");

    // Real posture 14 (DEAD) has no real corresponding .lat state - falls
    // through to the same default as any other unmapped posture.
    ctx.posture = 14;
    CHECK(std::string(stateNameFor(ctx)) == "loop_standing");

    // Real posture values without a dedicated real state yet (3=sneaking,
    // 9=skill-animating) fall back to standing, same as before.
    ctx.posture = 3;
    CHECK(std::string(stateNameFor(ctx)) == "loop_standing");
    ctx.posture = 9;
    CHECK(std::string(stateNameFor(ctx)) == "loop_standing");

    // Moving while kneeling/prone/sitting should NOT switch to the walk
    // state - only posture==0 (upright) does.
    ctx.posture = 1;
    ctx.isMoving = true;
    CHECK(std::string(stateNameFor(ctx)) == "loop_kneeling");
}

TEST_CASE("creatureanim::toSelectionContext - gender and real mood threaded through") {
    CreatureAnimationContext ctx;
    ctx.moodString = "angry";
    auto selection = toSelectionContext(ctx, "female");
    CHECK(selection.gender == "female");
    CHECK(selection.mood == "angry");
}

TEST_CASE("creatureanim::resolveAnimationContext - real CreatureObject with both baselines populated") {
    CreatureObject creature;
    creature.base3.emplace();
    creature.base3->posture = 2;
    creature.base3->stateBitmask =
        static_cast<uint64_t>(creatureanim::CreatureStateBit::Combat) |
        static_cast<uint64_t>(creatureanim::CreatureStateBit::Swimming);
    creature.base6.emplace();
    creature.base6->moodString = "nervous";
    creature.base6->weaponId = 281474976710656ull;
    creature.base6->performanceAnimation = "some_performance";

    auto ctx = resolveAnimationContext(creature, /*isMoving=*/true);
    CHECK(ctx.posture == 2);
    CHECK(ctx.isMoving == true);
    CHECK(ctx.moodString == "nervous");
    CHECK(ctx.weaponId == 281474976710656ull);
    CHECK(ctx.performanceAnimation == "some_performance");
    CHECK(ctx.inCombat());
    CHECK(ctx.isSwimming());
    CHECK(ctx.isArmed());
}

TEST_CASE("creatureanim::resolveAnimationContext - missing baselines fall back to real defaults") {
    CreatureObject creature; // base3/base6 both unset
    auto ctx = resolveAnimationContext(creature, /*isMoving=*/false);
    CHECK(ctx.posture == 0);
    CHECK(ctx.isMoving == false);
    CHECK(ctx.moodString.empty());
    CHECK(ctx.weaponId == 0);
    CHECK_FALSE(ctx.inCombat());
    CHECK_FALSE(ctx.isArmed());
}
