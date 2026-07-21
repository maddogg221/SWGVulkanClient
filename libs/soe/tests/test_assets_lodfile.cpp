// Permanent test for assets::LodFile against a real client .lod file,
// gated on the real client install being present (same pattern as
// test_assets_trearchive.cpp's live-archive test).
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/LodFile.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("LodFile: picks the _l0 (highest detail) mesh path from a real .lod file"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    TreArchive archive(kRealArchivePath);
    const std::string lodPath = "appearance/lod/thm_prp_crate_spice.lod";
    REQUIRE(archive.contains(lodPath));

    auto bytes = archive.extract(lodPath);
    auto data = LodFile::parse(bytes);
    CHECK(data.highestDetailMeshFilename == "mesh/thm_prp_crate_spice_l0.msh");
}
