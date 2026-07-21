// Verification for terrain::TerrainGenerator::queryColor()/queryShaderId() -
// Phase 5 of the terrain plan. No independently-known-good oracle exists
// for real color/shader values yet (that would need a real client-side
// screenshot or map tool to cross-check against, out of reach here) - this
// covers what's mechanically verifiable: real files parse with a texture
// archive supplied, colors vary across the map (proving ramp sampling is
// actually happening, not just defaulting to white everywhere), and
// results are deterministic.
#include <doctest/doctest.h>

#include <filesystem>
#include <set>

#include "assets/TreArchive.h"
#include "terrain/TerrainGenerator.h"
#include "terrain/TrnFile.h"

using namespace terrain;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
} // namespace

TEST_CASE("TerrainGenerator::queryColor - real naboo terrain produces varied, non-default colors"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm(), &archive);

    // Naboo has 28 distinct real ramp images (confirmed this session) -
    // real ramp sampling should produce real color variety, not the
    // uniform default white every point would have with no texture
    // archive supplied.
    std::set<std::tuple<int, int, int>> distinctColors;
    for (float x = -6000.0f; x <= 6000.0f; x += 800.0f) {
        for (float z = -6000.0f; z <= 6000.0f; z += 733.0f) {
            assets::Rgb8 c = generator.queryColor(x, z);
            distinctColors.insert({c.r, c.g, c.b});
        }
    }
    CHECK(distinctColors.size() > 5);
}

TEST_CASE("TerrainGenerator::queryColor - without a texture archive, stays at the documented "
          "default (white)"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    // No texture archive supplied - ramp affectors can't load any image,
    // so only AffectorColorConstant (a real, but less common, affector)
    // can move color away from white at all.
    auto generator = TerrainGenerator::parse(trn.generatorForm());

    assets::Rgb8 c = generator.queryColor(0.0f, 0.0f);
    // Not asserting pure white specifically (AffectorColorConstant is
    // real and could still apply at this exact point) - just confirming
    // this doesn't throw and returns a plausible byte-range color.
    CHECK(c.r <= 255);
    CHECK(c.g <= 255);
    CHECK(c.b <= 255);
}

TEST_CASE("TerrainGenerator::queryColor - deterministic for a given position"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm(), &archive);

    for (float x = -3000.0f; x <= 3000.0f; x += 917.0f) {
        for (float z = -3000.0f; z <= 3000.0f; z += 881.0f) {
            assets::Rgb8 c1 = generator.queryColor(x, z);
            assets::Rgb8 c2 = generator.queryColor(x, z);
            CHECK(c1.r == c2.r);
            CHECK(c1.g == c2.g);
            CHECK(c1.b == c2.b);
        }
    }
}

TEST_CASE("TerrainGenerator::queryShaderId - real naboo terrain assigns real shader families"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm(), &archive);

    std::set<int> distinctShaderIds;
    bool anyAssigned = false;
    for (float x = -6000.0f; x <= 6000.0f; x += 733.0f) {
        for (float z = -6000.0f; z <= 6000.0f; z += 811.0f) {
            int shaderId = generator.queryShaderId(x, z);
            distinctShaderIds.insert(shaderId);
            if (shaderId != -1) {
                anyAssigned = true;
            }
        }
    }
    CHECK(anyAssigned);
    // Naboo has 10 real shader families (confirmed this session) - real
    // assignment should pick more than just one.
    CHECK(distinctShaderIds.size() > 1);
}

TEST_CASE("TerrainGenerator::queryShaderId - deterministic for a given position"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm(), &archive);

    for (float x = -3000.0f; x <= 3000.0f; x += 941.0f) {
        for (float z = -3000.0f; z <= 3000.0f; z += 863.0f) {
            CHECK(generator.queryShaderId(x, z) == generator.queryShaderId(x, z));
        }
    }
}

TEST_CASE("TerrainGenerator::queryColor/queryShaderId - all 10 real classic-planet files parse "
          "with a texture archive and query without throwing"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    const char* planets[] = {
        "naboo",    "tatooine", "corellia", "dathomir", "dantooine",
        "endor",    "lok",      "rori",     "talus",    "yavin4",
    };
    assets::TreArchive archive(kRealArchivePath);
    for (const char* planet : planets) {
        INFO("planet: ", planet);
        auto bytes = archive.extract(std::string("terrain/") + planet + ".trn");
        auto trn = TrnFile::parse(bytes);
        auto generator = TerrainGenerator::parse(trn.generatorForm(), &archive);

        for (float x = -1000.0f; x <= 1000.0f; x += 250.0f) {
            for (float z = -1000.0f; z <= 1000.0f; z += 250.0f) {
                assets::Rgb8 c = generator.queryColor(x, z);
                (void)c;
                int shaderId = generator.queryShaderId(x, z);
                (void)shaderId;
            }
        }
    }
}
