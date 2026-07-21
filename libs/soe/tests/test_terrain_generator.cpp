// Verification for the full TGEN structural parse (terrain::TerrainGenerator,
// via Boundary/Filter/Affector/Layer/Family) - Phase 3 of the terrain plan.
// Requires the real client install to be present on this machine (same
// archive libs/assets' own tests already gate on) - skipped, not failed,
// wherever it isn't.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/TreArchive.h"
#include "terrain/TerrainGenerator.h"
#include "terrain/TrnFile.h"

using namespace terrain;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";

const char* kAllPlanetFiles[] = {
    "terrain/naboo.trn",     "terrain/tatooine.trn", "terrain/corellia.trn",
    "terrain/dathomir.trn",  "terrain/dantooine.trn", "terrain/endor.trn",
    "terrain/lok.trn",       "terrain/rori.trn",      "terrain/talus.trn",
    "terrain/yavin4.trn",
};

// Real per-planet top-level layer counts, dumped from this exact archive
// this session (see the terrain Phase 3 commit) via a temporary raw-tag
// scanner (check_tgen, since removed) - pinned rather than just
// sanity-checked, matching TrnFile's own header-value verification bar.
// naboo's count (28) was also independently confirmed during Phase 11's
// research pass via direct extraction.
struct ExpectedLayerCount {
    const char* file;
    int topLevelLayers;
};
const ExpectedLayerCount kExpectedLayerCounts[] = {
    {"terrain/naboo.trn", 28},    {"terrain/tatooine.trn", 27}, {"terrain/corellia.trn", 17},
    {"terrain/dathomir.trn", 14}, {"terrain/dantooine.trn", 17}, {"terrain/endor.trn", 22},
    {"terrain/lok.trn", 22},      {"terrain/rori.trn", 19},      {"terrain/talus.trn", 23},
    {"terrain/yavin4.trn", 7},
};

int countLayersRecursive(const std::vector<Layer>& layers) {
    int count = static_cast<int>(layers.size());
    for (const auto& layer : layers) {
        count += countLayersRecursive(layer.subLayers);
    }
    return count;
}

} // namespace

TEST_CASE("TerrainGenerator::parse - all 10 real classic-planet .trn files parse without throwing"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    for (const char* name : kAllPlanetFiles) {
        INFO("planet file: ", name);
        REQUIRE(archive.contains(name));
        auto bytes = archive.extract(name);
        auto trn = TrnFile::parse(bytes);
        auto generator = TerrainGenerator::parse(trn.generatorForm());

        // Every real file has at least one shader family and at least one
        // top-level layer - a basic sanity floor, not a real-bytes pin.
        CHECK(!generator.families.shaderFamilies.empty());
        CHECK(!generator.topLevelLayers.empty());
    }
}

TEST_CASE("TerrainGenerator::parse - real per-planet top-level layer counts match exactly"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    for (const auto& expected : kExpectedLayerCounts) {
        INFO("planet file: ", expected.file);
        auto bytes = archive.extract(expected.file);
        auto trn = TrnFile::parse(bytes);
        auto generator = TerrainGenerator::parse(trn.generatorForm());
        CHECK(static_cast<int>(generator.topLevelLayers.size()) == expected.topLevelLayers);
    }
}

TEST_CASE("TerrainGenerator::parse - naboo real family/layer counts"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm());

    CHECK(generator.topLevelLayers.size() == 28);
    // Real naboo.trn has exactly one MGRP (fractal families only, bitmap
    // group absent) - confirmed both by this session's research and the
    // check_tgen scan.
    CHECK(!generator.families.fractalFamilies.empty());
    CHECK(generator.families.bitmapFamilies.empty());
    CHECK(!generator.families.shaderFamilies.empty());
    CHECK(!generator.families.floraFamilies.empty());
    CHECK(!generator.families.radialFamilies.empty());
    CHECK(!generator.families.environmentFamilies.empty());

    int totalLayers = countLayersRecursive(generator.topLevelLayers);
    CHECK(totalLayers > 28); // real tree has nested sub-layers too
}

TEST_CASE("TerrainGenerator::parse - total layer count across all 10 files matches real-bytes scan"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    int totalLayers = 0;
    for (const char* name : kAllPlanetFiles) {
        auto bytes = archive.extract(name);
        auto trn = TrnFile::parse(bytes);
        auto generator = TerrainGenerator::parse(trn.generatorForm());
        totalLayers += countLayersRecursive(generator.topLevelLayers);
    }
    // Matches check_tgen's own tally exactly (2054 total layers including
    // nested sub-layers, across all 10 classic planets).
    CHECK(totalLayers == 2054);
}
