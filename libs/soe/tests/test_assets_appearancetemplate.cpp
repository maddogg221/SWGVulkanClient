// Permanent test for assets::AppearanceTemplate against a real client .apt
// file, gated on the real client install being present (same pattern as
// test_assets_trearchive.cpp's live-archive test).
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/AppearanceTemplate.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("AppearanceTemplate: reads the referenced filename from a real .apt file"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    TreArchive archive(kRealArchivePath);
    const std::string aptPath = "appearance/thm_prp_crate_spice.apt";
    REQUIRE(archive.contains(aptPath));

    auto bytes = archive.extract(aptPath);
    auto data = AppearanceTemplate::parse(bytes);
    CHECK(data.referencedFilename == "appearance/lod/thm_prp_crate_spice.lod");
}
