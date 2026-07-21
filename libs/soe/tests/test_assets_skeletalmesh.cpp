// Permanent tests for assets::SkeletalMeshLod and assets::SkeletalMesh
// against real client files, gated on the real client install being
// present (same pattern as test_assets_lodfile.cpp's live-archive test).
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/SkeletalMesh.h"
#include "assets/SkeletalMeshLod.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kOtherArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
const char* kSkeletalMesh01ArchivePath =
    "C:\\Program Files (x86)\\StarWarsGalaxies\\data_skeletal_mesh_01.tre";
} // namespace

// Real, confirmed second triangle-list format: a real per-shader submesh's
// FORM PRIM can contain 'OITL' (grouped/"organized" triangle list - a
// per-triangle int16 group tag, discarded here, ahead of the same 3x
// uint32 vertex-index triple 'ITL ' uses) instead of plain 'ITL '. Found
// live (2026-07-19) via a real Naritus test: every zab_m_*.mgn player-race
// body-part mesh uses OITL, while the simpler creature meshes above (gubbur,
// tanc_mite) use plain ITL - both are real, not a version difference to
// pick one over the other.
TEST_CASE("SkeletalMesh: parses a real submesh using OITL (grouped triangle list) instead of ITL"
          * doctest::skip(!std::filesystem::exists(kSkeletalMesh01ArchivePath))) {
    TreArchive archive(kSkeletalMesh01ArchivePath);
    const std::string mgnPath = "appearance/mesh/zab_m_arms_l0.mgn";
    REQUIRE(archive.contains(mgnPath));

    auto bytes = archive.extract(mgnPath);
    auto mesh = SkeletalMesh::parse(bytes);

    CHECK(mesh.boneNames.size() == 8);
    CHECK(mesh.vertexWeights.size() == 96);
    REQUIRE(mesh.submeshes.size() == 1);

    const auto& submesh = mesh.submeshes[0];
    CHECK(submesh.positions.size() == 110);
    CHECK(submesh.indices.size() == 154 * 3);
    for (uint32_t index : submesh.indices) {
        CHECK(index < submesh.positions.size());
    }
}

TEST_CASE("SkeletalMeshLod: picks the _l0 (highest detail) mesh path from a real .lmg file"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string lmgPath = "appearance/mesh/gubbur.lmg";
    REQUIRE(archive.contains(lmgPath));

    auto bytes = archive.extract(lmgPath);
    auto data = SkeletalMeshLod::parse(bytes);
    // Real .lmg NAME entries are full archive paths, unlike .lod's
    // relative-to-"appearance/" CHLD entries.
    CHECK(data.highestDetailMeshFilename == "appearance/mesh/gubbur_l0.mgn");
}

// Real, confirmed anomaly: gubbur_l0.mgn has zero PSDT (shader/submesh)
// blocks - a genuine, unused/placeholder creature mesh, not a parse
// failure. SkeletalMesh::parse() must handle this gracefully (empty
// `submeshes`, no throw), since this is exactly the case the skeletal-mesh
// resolver's wireframe-box fallback needs to detect and handle.
TEST_CASE("SkeletalMesh: a real mesh with zero PSDT blocks parses with empty submeshes, not an error"
          * doctest::skip(!std::filesystem::exists(
              "C:\\Program Files (x86)\\StarWarsGalaxies\\data_skeletal_mesh_00.tre"))) {
    TreArchive archive("C:\\Program Files (x86)\\StarWarsGalaxies\\data_skeletal_mesh_00.tre");
    const std::string mgnPath = "appearance/mesh/gubbur_l0.mgn";
    REQUIRE(archive.contains(mgnPath));

    auto bytes = archive.extract(mgnPath);
    auto mesh = SkeletalMesh::parse(bytes);
    CHECK(mesh.skeletonFilename == "appearance/skeleton/frog.skt");
    CHECK(mesh.boneNames.size() == 29);
    CHECK(mesh.vertexWeights.size() == 509);
    CHECK(mesh.submeshes.empty());
}

TEST_CASE("SkeletalMesh: parses real per-shader submesh geometry from a real, non-degenerate .mgn file"
          * doctest::skip(!std::filesystem::exists(kSkeletalMesh01ArchivePath))) {
    TreArchive archive(kSkeletalMesh01ArchivePath);
    const std::string mgnPath = "appearance/mesh/tanc_mite_l0.mgn";
    REQUIRE(archive.contains(mgnPath));

    auto bytes = archive.extract(mgnPath);
    auto mesh = SkeletalMesh::parse(bytes);

    CHECK(mesh.skeletonFilename == "appearance/skeleton/insect_basic.skt");
    CHECK(mesh.boneNames.size() == 43);
    CHECK(mesh.vertexWeights.size() == 970);
    REQUIRE(mesh.submeshes.size() == 1);

    const auto& submesh = mesh.submeshes[0];
    CHECK(submesh.shaderFilename == "shader/tanc_mite_as9.sht");
    CHECK(submesh.positions.size() == 1449);
    CHECK(submesh.normals.size() == 1449);
    CHECK(submesh.uv0.size() == 1449);
    CHECK(submesh.indices.size() == 1936 * 3);

    // Every index must reference a real local vertex - the strongest
    // end-to-end sanity check that PIDX/NIDX/ITL's real two-level
    // indirection was resolved correctly, not just that decoding didn't
    // crash.
    for (uint32_t index : submesh.indices) {
        CHECK(index < submesh.positions.size());
    }

    // Normals should be unit-length (within float tolerance) - an
    // independent sanity check that NIDX's byte offsets are correct.
    for (const auto& n : submesh.normals) {
        float lengthSq = n.x * n.x + n.y * n.y + n.z * n.z;
        CHECK(lengthSq == doctest::Approx(1.0f).epsilon(0.01));
    }

    // At least one real vertex should have more than one bone weight -
    // confirms TWHD/TWDT's real per-vertex weight counts were read (not
    // just a fixed one-bone-per-vertex assumption).
    bool sawMultiWeightVertex = false;
    for (const auto& weights : mesh.vertexWeights) {
        if (weights.size() > 1) {
            sawMultiWeightVertex = true;
            break;
        }
    }
    CHECK(sawMultiWeightVertex);
}
