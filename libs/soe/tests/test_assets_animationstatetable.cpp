// Permanent test for assets::AnimationStateTable against a real client .lat
// file, gated on the real client install being present (same pattern as
// test_assets_skeletalappearance.cpp's live-archive test).
#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>

#include "assets/AnimationStateTable.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kOtherArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("AnimationStateTable: parses real named animation states from a real .lat file"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string latPath = "appearance/lat/all_m.lat";
    REQUIRE(archive.contains(latPath));

    auto bytes = archive.extract(latPath);
    auto table = AnimationStateTable::parse(bytes);

    // Real file has 664 real FORM(ANIM) states for the base humanoid
    // skeleton (confirmed via the outer FORM(0000)'s own real
    // [665 children] count, minus its own leading CHNK INFO).
    CHECK(table.states.size() == 664);

    // First real state in file order - a simple single-clip FORM(PXAT)
    // case, parsing directly as a root Clip node (no Switch/Container).
    REQUIRE(!table.states.empty());
    CHECK(table.states[0].name == "pistol_combat_prone_fire_9");
    CHECK(table.states[0].root.kind == AnimationNodeKind::Clip);
    CHECK(table.states[0].root.clipPath == "appearance/animation/all_b_cbt_pistol_prone_ready_fire_9.ans");

    // A real transition state, exactly matching the names the user asked
    // about (posture-to-posture transitions) - proves real trn_* states
    // are present and parse like any other single-clip state.
    auto trnIt = std::find_if(table.states.begin(), table.states.end(), [](const AnimationState& s) {
        return s.name == "trn_pistol_combat_kneeling_to_pistol_combat_kneeling_aimed";
    });
    REQUIRE(trnIt != table.states.end());
    CHECK(trnIt->root.kind == AnimationNodeKind::Clip);
    CHECK(trnIt->root.clipPath == "appearance/animation/all_b_cbt_pistol_kneeling_ready_to_aimed.ans");

    // A real multi-variant locomotion state - proves the real recursive
    // tree (not a flattened list) is built: a real Container holding one
    // Switch (the mood tree) plus real locomotion-tagged Clip siblings,
    // and that selectAnimationClip() picks a sensible, deterministic
    // result for both a resting and a movement selection.
    auto locIt = std::find_if(table.states.begin(), table.states.end(), [](const AnimationState& s) {
        return s.name == "loop_combat_standing";
    });
    REQUIRE(locIt != table.states.end());
    CHECK(locIt->root.kind == AnimationNodeKind::Container);
    bool hasLocomotionChild = std::any_of(locIt->root.children.begin(), locIt->root.children.end(),
                                           [](const AnimationNode& n) {
                                               return n.kind == AnimationNodeKind::Clip &&
                                                      n.punfParameterName == "locomotion";
                                           });
    CHECK(hasLocomotionChild);

    AnimationSelectionContext maleCtx{"male"};
    CHECK(selectAnimationClip(locIt->root, maleCtx, /*preferLocomotion=*/true) ==
          "appearance/animation/all_b_loc_walk_male.ans");
    CHECK(selectAnimationClip(locIt->root, maleCtx, /*preferLocomotion=*/false) ==
          "appearance/animation/all_b_cbt_unarmed_standing_ready_idle_3.ans");

    // A real gender-branched state - proves the top-level real "gender"
    // Switch is parsed and selected correctly for both real values, and
    // that the resting selection consistently lands on a real neutral
    // idle clip regardless of which branch was chosen (confirmed real:
    // the same real "breathe calmly" clip is each branch's own real
    // first mood-tree entry).
    auto standIt = std::find_if(table.states.begin(), table.states.end(),
                                 [](const AnimationState& s) { return s.name == "loop_standing"; });
    REQUIRE(standIt != table.states.end());
    CHECK(standIt->root.kind == AnimationNodeKind::Switch);
    CHECK(standIt->root.parameterName == "gender");
    REQUIRE(standIt->root.children.size() == 2);

    AnimationSelectionContext femaleCtx{"female"};
    CHECK(selectAnimationClip(standIt->root, maleCtx, false) ==
          "appearance/animation/all_b_idl_breathe_calmly.ans");
    CHECK(selectAnimationClip(standIt->root, femaleCtx, false) ==
          "appearance/animation/all_b_idl_breathe_calmly.ans");
    CHECK(selectAnimationClip(standIt->root, maleCtx, true) ==
          "appearance/animation/all_b_loc_walk_male.ans");
}
