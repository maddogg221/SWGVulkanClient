// Verification for assets::TgaImage - the minimal 24bpp/single-row TGA
// reader built for terrain color-ramp images (AffectorColorRampHeight/
// AffectorColorRampFractal). Requires the real client install to be
// present - skipped, not failed, wherever it isn't.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/TgaImage.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
} // namespace

TEST_CASE("TgaImage::parse - real naboo color-ramp images parse with the exact expected widths"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    TreArchive archive(kRealArchivePath);

    // Real widths, confirmed this session by decoding these exact files'
    // TGA headers byte-for-byte (18-byte header + width*3 pixel bytes +
    // 26-byte TGA 2.0 footer == the real file size in every case).
    struct Expected {
        const char* file;
        int width;
    };
    const Expected files[] = {
        {"terrain/naboo_colorramp_beach.tga", 32},
        {"terrain/naboo_colorramp_grass_city.tga", 64},
        {"terrain/naboo_colorramp_mtns_gallo.tga", 128},
        {"terrain/tals_colorramp_oceanfloor.tga", 256},
    };

    for (const auto& f : files) {
        INFO("file: ", f.file);
        REQUIRE(archive.contains(f.file));
        auto bytes = archive.extract(f.file);
        auto image = TgaImage::parse(bytes);
        CHECK(image.width == f.width);
        CHECK(static_cast<int>(image.pixels.size()) == f.width);
    }
}
