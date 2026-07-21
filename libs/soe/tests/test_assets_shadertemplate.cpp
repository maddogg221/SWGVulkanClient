// Permanent test for assets::ShaderTemplate against real client .sht files,
// gated on the real client install being present (same pattern as
// test_assets_staticmesh.cpp's live-archive test). Confirmed via a real
// chunk-tree dump that a real texture slot's tag is stored byte-reversed on
// disk (a real "MAIN" slot's own DATA chunk literally starts with the bytes
// "NIAM") - see ShaderTemplate.cpp's own comment for the fuller trace.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/ShaderTemplate.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kOtherArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("ShaderTemplate: parses a real SSHT .sht file's main texture"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string shtPath = "shader/thm_prp_crate_spice.sht";
    REQUIRE(archive.contains(shtPath));

    auto bytes = archive.extract(shtPath);
    auto sht = ShaderTemplate::parse(bytes);
    CHECK(sht.mainTextureFilename == "texture/thm_prp_crate_spice.dds");
}

TEST_CASE("ShaderTemplate: parses real per-cell building shader files"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);

    {
        const std::string shtPath = "shader/intr_assoc_marblered_ces17.sht";
        REQUIRE(archive.contains(shtPath));
        auto sht = ShaderTemplate::parse(archive.extract(shtPath));
        CHECK(sht.mainTextureFilename == "texture/intr_assoc_marblered.dds");
    }
    {
        const std::string shtPath = "shader/intr_medicalroof_a_cs8.sht";
        REQUIRE(archive.contains(shtPath));
        auto sht = ShaderTemplate::parse(archive.extract(shtPath));
        CHECK(sht.mainTextureFilename == "texture/intr_medicalroof_a.dds");
    }
}
