// Permanent test for assets::AnimationClip against a real client .ans file,
// gated on the real client install being present (same pattern as
// test_assets_skeletalappearance.cpp's live-archive test). The clip file
// itself lives in data_animation_00.tre, a previously-unopened archive
// discovered during Phase 21 (animation) research.
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>

#include "assets/AnimationClip.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kAnimationArchivePath =
    "C:\\Program Files (x86)\\StarWarsGalaxies\\data_animation_00.tre";

float quatLength(const Quaternion& q) {
    return std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
}
} // namespace

TEST_CASE("AnimationClip: parses real per-bone keyframe data from a real .ans file"
          * doctest::skip(!std::filesystem::exists(kAnimationArchivePath))) {
    TreArchive archive(kAnimationArchivePath);
    const std::string ansPath = "appearance/animation/all_b_prone_to_standing.ans";
    REQUIRE(archive.contains(ansPath));

    auto bytes = archive.extract(ansPath);
    auto clip = AnimationClip::parse(bytes);

    // Real file has 42 real per-bone XFIN records.
    REQUIRE(clip.bones.size() == 42);

    // bones[0] = "lthumb02", a rotation-animated bone whose real rotation
    // channel index (0) selects the first real QCHN chunk (15 real
    // keyframes).
    CHECK(clip.bones[0].boneName == "lthumb02");
    CHECK(clip.bones[0].rotationKeyframes.size() == 15);
    CHECK(clip.bones[0].translationAxis[0].empty());
    CHECK(clip.bones[0].translationAxis[1].empty());
    CHECK(clip.bones[0].translationAxis[2].empty());

    // bones[5] = "sword", a real "hardpoint" attachment bone with no
    // ANIMATED rotation channel and no translation channel - but (found
    // 2026-07-28 while exhausting the .ans format for the Phase 21
    // rest-pose investigation) it DOES have a real per-clip STATIC
    // rotation (CHNK SROT), a tiny ~2.8deg offset from bind pose, not the
    // identity/no-data this project previously assumed for every
    // hasRotation==0 bone.
    CHECK(clip.bones[5].boneName == "sword");
    CHECK(clip.bones[5].rotationKeyframes.empty());
    CHECK(clip.bones[5].translationAxis[0].empty());
    CHECK(clip.bones[5].hasStaticRotation);
    CHECK(clip.bones[5].staticRotation.w == doctest::Approx(0.999734f).epsilon(0.001));

    // bones[7] = "root", the one real bone confirmed (this session) to
    // carry translation - real per-axis keyframe counts differ (27/28/28),
    // confirming the three axes are independent real curves, not a shared
    // Float3 channel. Has a real ANIMATED rotation channel, so it should
    // NOT also carry a static rotation (the two are mutually exclusive per
    // XFIN's own `hasRotation` flag).
    CHECK(clip.bones[7].boneName == "root");
    CHECK(clip.bones[7].rotationKeyframes.size() == 28);
    CHECK_FALSE(clip.bones[7].hasStaticRotation);
    REQUIRE(clip.bones[7].translationAxis[0].size() == 27);
    REQUIRE(clip.bones[7].translationAxis[1].size() == 28);
    REQUIRE(clip.bones[7].translationAxis[2].size() == 28);

    // Real structural count across the whole file: 7 of the 42 real bones
    // have a static (non-animated) rotation, matching the real per-bone
    // XFIN hasRotation==0 count found live this session.
    int staticRotationCount = 0;
    for (const auto& bone : clip.bones) {
        if (bone.hasStaticRotation) {
            ++staticRotationCount;
        }
    }
    CHECK(staticRotationCount == 7);

    // Real translation values are plain, un-quantized floats in a small,
    // physically-plausible meter range for a root bone's motion during a
    // prone-to-standing clip.
    CHECK(clip.bones[7].translationAxis[0][0].value == doctest::Approx(0.01494f).epsilon(0.01));

    // Structural invariant on the real quantized rotation decode (mirrors
    // test_assets_skeleton.cpp's own unit-quaternion sanity check): the
    // large majority of real keyframes across every animated bone should
    // reconstruct to an approximately unit-length quaternion. Not literally
    // every one (this session's research found ~98% across all 35 real
    // channels of this exact file) - real quantization can occasionally
    // land right at/outside the valid boundary.
    int total = 0;
    int nearUnit = 0;
    for (const auto& bone : clip.bones) {
        for (const auto& kf : bone.rotationKeyframes) {
            ++total;
            if (std::fabs(quatLength(kf.rotation) - 1.0f) < 0.05f) {
                ++nearUnit;
            }
        }
    }
    REQUIRE(total > 0);
    CHECK(static_cast<double>(nearUnit) / total > 0.95);
}
