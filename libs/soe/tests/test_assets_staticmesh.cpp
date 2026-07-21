// Permanent test for assets::StaticMesh against a real client .msh file,
// gated on the real client install being present (same pattern as
// test_assets_trearchive.cpp's live-archive test). The 32-byte vertex
// layout (position/normal/UV0, no color, one UV pair) is this format's
// simplest, fully unambiguous case, confirmed against meshLib's own
// mshVertex::getPosition()/getNormal()/getTexCoords()
// (github.com/apathyboy/meshlib) - see StaticMesh.cpp's own comment for
// the fuller trace, including this real file's exact fvfCodes/numVerts
// values found by directly dumping its chunk tree.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/AppearanceTemplate.h"
#include "assets/LodFile.h"
#include "assets/StaticMesh.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kMeshArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_static_mesh_01.tre";
}

TEST_CASE("StaticMesh: parses real vertex/index data from a real .msh file"
          * doctest::skip(!std::filesystem::exists(kMeshArchivePath))) {
    TreArchive archive(kMeshArchivePath);
    const std::string meshPath = "appearance/mesh/thm_prp_crate_spice_l0.msh";
    REQUIRE(archive.contains(meshPath));

    auto bytes = archive.extract(meshPath);
    auto submeshes = StaticMesh::parse(bytes);
    REQUIRE(!submeshes.empty());
    const auto& mesh = submeshes[0].mesh;

    CHECK(mesh.positions.size() == 441);
    CHECK(mesh.normals.size() == 441);
    CHECK(mesh.uv0.size() == 441);
    CHECK(mesh.indices.size() == 732); // 244 triangles

    // Every index must reference a real vertex - the single strongest
    // end-to-end sanity check that vertex count/index parsing agree with
    // each other, not just with themselves.
    for (uint32_t index : mesh.indices) {
        CHECK(index < mesh.positions.size());
    }

    // Normals should be unit-length (within float tolerance) - a real,
    // independent sanity check that the byte offsets used to extract them
    // are correct, not just that decoding didn't crash.
    for (const auto& n : mesh.normals) {
        float lengthSq = n.x * n.x + n.y * n.y + n.z * n.z;
        CHECK(lengthSq == doctest::Approx(1.0f).epsilon(0.01));
    }
}

// End-to-end resolution chain test: .apt -> .lod -> real mesh data, proving
// every hop this session discovered composes correctly against real files
// spread across two different archives (data_other_00.tre for the
// .apt/.lod, data_static_mesh_01.tre for the .msh itself).
TEST_CASE("End-to-end: .apt -> .lod -> real StaticMesh geometry"
          * doctest::skip(!std::filesystem::exists(
              "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre") ||
                           !std::filesystem::exists(kMeshArchivePath))) {
    TreArchive otherArchive("C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre");
    auto aptBytes = otherArchive.extract("appearance/thm_prp_crate_spice.apt");
    auto apt = AppearanceTemplate::parse(aptBytes);
    REQUIRE(apt.referencedFilename == "appearance/lod/thm_prp_crate_spice.lod");

    auto lodBytes = otherArchive.extract(apt.referencedFilename);
    auto lod = LodFile::parse(lodBytes);
    REQUIRE(lod.highestDetailMeshFilename == "mesh/thm_prp_crate_spice_l0.msh");

    std::string fullMeshPath = "appearance/" + lod.highestDetailMeshFilename;
    TreArchive meshArchive(kMeshArchivePath);
    REQUIRE(meshArchive.contains(fullMeshPath));
    auto meshBytes = meshArchive.extract(fullMeshPath);
    auto submeshes = StaticMesh::parse(meshBytes);
    REQUIRE(!submeshes.empty());
    CHECK(submeshes[0].mesh.positions.size() == 441);
    CHECK(!submeshes[0].mesh.indices.empty());
}
