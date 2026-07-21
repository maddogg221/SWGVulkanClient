// Permanent test for assets::SkeletalAppearance against a real client .sat
// file, gated on the real client install being present (same pattern as
// test_assets_lodfile.cpp's live-archive test).
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/SharedObjectTemplate.h"
#include "assets/SkeletalAppearance.h"
#include "assets/SkeletalMesh.h"
#include "assets/SkeletalMeshLod.h"
#include "assets/Skeleton.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kOtherArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("SkeletalAppearance: parses real mesh/skeleton references from a real .sat file"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string satPath = "appearance/gubbur.sat";
    REQUIRE(archive.contains(satPath));

    auto bytes = archive.extract(satPath);
    auto appearance = SkeletalAppearance::parse(bytes);

    REQUIRE(appearance.meshFilenames.size() == 1);
    CHECK(appearance.meshFilenames[0] == "appearance/mesh/gubbur.lmg");
    CHECK(appearance.skeletonFilename == "appearance/skeleton/frog.skt");
}

// End-to-end resolution chain test: .iff -> .sat -> [.lmg -> .mgn] + .skt,
// proving every real hop this phase's research discovered composes
// correctly against real files spread across three archives
// (data_other_00.tre for the .iff/.sat/.lmg/.skt, data_skeletal_mesh_00.tre
// for the .mgn itself). Mirrors test_assets_staticmesh.cpp's own
// ".apt -> .lod -> real StaticMesh geometry" end-to-end test.
TEST_CASE("End-to-end: .iff -> .sat -> .lmg -> real SkeletalMesh geometry, plus .skt skeleton"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath) ||
                           !std::filesystem::exists("C:\\Program Files (x86)\\StarWarsGalaxies\\"
                                                     "data_skeletal_mesh_00.tre"))) {
    TreArchive otherArchive(kOtherArchivePath);
    auto iffBytes = otherArchive.extract("object/mobile/shared_gubbur.iff");
    auto tmpl = SharedObjectTemplate::parse(iffBytes);
    REQUIRE(tmpl.appearanceFilename == "appearance/gubbur.sat");

    auto satBytes = otherArchive.extract(tmpl.appearanceFilename);
    auto appearance = SkeletalAppearance::parse(satBytes);
    REQUIRE(appearance.meshFilenames.size() == 1);

    auto lmgBytes = otherArchive.extract(appearance.meshFilenames[0]);
    auto lod = SkeletalMeshLod::parse(lmgBytes);
    REQUIRE(lod.highestDetailMeshFilename == "appearance/mesh/gubbur_l0.mgn");

    TreArchive meshArchive("C:\\Program Files (x86)\\StarWarsGalaxies\\data_skeletal_mesh_00.tre");
    REQUIRE(meshArchive.contains(lod.highestDetailMeshFilename));
    auto mgnBytes = meshArchive.extract(lod.highestDetailMeshFilename);
    auto mesh = SkeletalMesh::parse(mgnBytes);
    CHECK(mesh.vertexWeights.size() == 509);
    // gubbur's own l0 mesh is the real zero-PSDT anomaly - the resolver's
    // fallback path handles this, not this test's job to work around it.
    CHECK(mesh.submeshes.empty());

    REQUIRE(appearance.skeletonFilename == "appearance/skeleton/frog.skt");
    auto sktBytes = otherArchive.extract(appearance.skeletonFilename);
    auto skeleton = Skeleton::parse(sktBytes);
    CHECK(skeleton.bones.size() == 30);
}
