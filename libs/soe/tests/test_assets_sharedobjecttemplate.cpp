// Permanent test for assets::SharedObjectTemplate against a real client
// .iff template, gated on the real client install being present (same
// pattern as test_assets_trearchive.cpp's live-archive test - skipped, not
// failed, on any machine without it). The 'XXXX' self-describing parameter
// chunk format (name then value, both NUL-terminated strings) was
// discovered by directly dumping this exact file's chunk tree - not
// documented anywhere - so this test is the permanent record proving that
// discovery against real bytes, not just a synthetic fixture.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/SharedObjectTemplate.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("SharedObjectTemplate: reads appearanceFilename from a real .iff template"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    TreArchive archive(kRealArchivePath);
    const std::string templatePath = "object/static/item/shared_item_crate_spice.iff";
    REQUIRE(archive.contains(templatePath));

    auto bytes = archive.extract(templatePath);
    auto data = SharedObjectTemplate::parse(bytes);
    CHECK(data.appearanceFilename == "appearance/thm_prp_crate_spice.apt");
}
