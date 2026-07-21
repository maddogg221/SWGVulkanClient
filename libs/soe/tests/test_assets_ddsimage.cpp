// Permanent test for assets::DdsImage against real client .dds files, gated
// on the real client install being present (same pattern as
// test_assets_shadertemplate.cpp's live-archive test). Confirmed via a real
// header dump that this project's own client archives only use FourCC
// DXT1/DXT5 (BC1/BC3) - see DdsImage.cpp's own comment for the exact real
// field offsets.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/DdsImage.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kTextureArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_texture_06.tre";
const char* kTextureArchivePath3 = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_texture_03.tre";
}

TEST_CASE("DdsImage: parses a real DXT1 (BC1) texture"
          * doctest::skip(!std::filesystem::exists(kTextureArchivePath))) {
    TreArchive archive(kTextureArchivePath);
    const std::string ddsPath = "texture/thm_prp_crate_spice.dds";
    REQUIRE(archive.contains(ddsPath));

    auto bytes = archive.extract(ddsPath);
    auto dds = DdsImage::parse(bytes);

    CHECK(dds.width == 256);
    CHECK(dds.height == 256);
    CHECK(dds.format == DdsBlockFormat::Bc1);
    // BC1: 8 bytes per 4x4 block, 64x64 blocks for a 256x256 base mip.
    CHECK(dds.blockData.size() == 64u * 64u * 8u);
}

TEST_CASE("DdsImage: parses a real DXT5 (BC3) texture"
          * doctest::skip(!std::filesystem::exists(kTextureArchivePath3))) {
    TreArchive archive(kTextureArchivePath3);
    const std::string ddsPath = "texture/intr_assoc_marblered.dds";
    REQUIRE(archive.contains(ddsPath));

    auto bytes = archive.extract(ddsPath);
    auto dds = DdsImage::parse(bytes);

    CHECK(dds.width == 256);
    CHECK(dds.height == 256);
    CHECK(dds.format == DdsBlockFormat::Bc3);
    // BC3: 16 bytes per 4x4 block, 64x64 blocks for a 256x256 base mip.
    CHECK(dds.blockData.size() == 64u * 64u * 16u);
}

TEST_CASE("DdsImage: parses a real non-power-of-two-height DXT5 texture") {
    // Same archive/file as above but a real non-square texture
    // (64x256) - confirms the block-rounding math for a dimension that
    // isn't a multiple of the other.
    if (!std::filesystem::exists(kTextureArchivePath3)) {
        return;
    }
    TreArchive archive(kTextureArchivePath3);
    const std::string ddsPath = "texture/intr_medicallight_a.dds";
    REQUIRE(archive.contains(ddsPath));

    auto bytes = archive.extract(ddsPath);
    auto dds = DdsImage::parse(bytes);

    CHECK(dds.width == 64);
    CHECK(dds.height == 256);
    CHECK(dds.format == DdsBlockFormat::Bc3);
    CHECK(dds.blockData.size() == 16u * 64u * 16u);
}
