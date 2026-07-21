// Permanent test for assets::Skeleton against a real client .skt file,
// gated on the real client install being present (same pattern as
// test_assets_lodfile.cpp's live-archive test).
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/Skeleton.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kOtherArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("Skeleton: picks the highest-bone-count FORM SKTM block from a real .skt file"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string sktPath = "appearance/skeleton/frog.skt";
    REQUIRE(archive.contains(sktPath));

    auto bytes = archive.extract(sktPath);
    auto skeleton = Skeleton::parse(bytes);

    // Real file has 4 nested FORM SKTM blocks (30/24/17/7 bones) - the
    // 30-bone block must be selected.
    CHECK(skeleton.bones.size() == 30);
    CHECK(skeleton.bones[0].name == "root");
    // Every non-root bone's parent index must reference an earlier or
    // equal-or-later real bone slot - the strongest simple sanity check
    // that PRNT was read at the right offset/stride, not just that
    // decoding didn't crash.
    for (size_t i = 0; i < skeleton.bones.size(); ++i) {
        int32_t parent = skeleton.bones[i].parentIndex;
        CHECK((parent == -1 || (parent >= 0 && parent < static_cast<int32_t>(skeleton.bones.size()))));
    }

    // Quaternions should be unit-length (within float tolerance) - an
    // independent sanity check on RPRE's byte offsets.
    for (const auto& bone : skeleton.bones) {
        const auto& q = bone.preRotation;
        float lengthSq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        CHECK(lengthSq == doctest::Approx(1.0f).epsilon(0.01));
    }
}
