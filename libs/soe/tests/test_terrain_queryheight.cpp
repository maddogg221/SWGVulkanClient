// Verification for terrain::TerrainGenerator::queryHeight() - Phase 4 of
// the terrain plan.
//
// Includes the plan's intended strongest verification: real
// (worldX, worldZ) -> real ground Y samples, captured live via tshark +
// pcapdecoder (CmdStartScene) against both a private dev server and
// Finalizer in the same session, with the local server sample independently
// confirmed by the user as standing on open outdoor terrain (a second,
// indoor/cell-relative sample from the same session is kept too, as a
// deliberate negative control - see the last real-position test below).
//
// This live session also caught a real bug: swgproto::CmdStartScene's x/y/z
// field semantics didn't match this codebase's own established convention
// (y=height) - Core3's own CmdStartScene.h sends a raw Vector3's
// getX()/getZ()/getY() in that order, and Vector3::getZ() is the one that's
// actually vertical height, not getY(). Never caught before because
// CmdStartScene's position was only ever printed, never rendered (unlike
// every other position type in this codebase, which IS rendered and was
// thus implicitly cross-checked every time the visualizer ran). Fixed in
// swgproto/CmdStartScene.h/.cpp (field rename only, wire read order
// unchanged) - see that header's own comment for the full story.
#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>

#include "assets/TreArchive.h"
#include "terrain/TerrainGenerator.h"
#include "terrain/TrnFile.h"

using namespace terrain;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
} // namespace

TEST_CASE("TerrainGenerator::queryHeight - naboo real file produces finite, stable heights"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm());

    // Naboo's map is 16384m wide, centered on the origin (confirmed real
    // header value from Phase 1) - sample a spread of in-bounds points.
    for (float x = -8000.0f; x <= 8000.0f; x += 653.0f) {
        for (float z = -8000.0f; z <= 8000.0f; z += 733.0f) {
            float h = generator.queryHeight(x, z);
            CAPTURE(x);
            CAPTURE(z);
            CHECK(std::isfinite(h));
            // Naboo has no space/orbital content and no known terrain
            // extremes anywhere near this range - a generous sanity bound,
            // not a real-bytes pin.
            CHECK(h > -1000.0f);
            CHECK(h < 2000.0f);
        }
    }
}

TEST_CASE("TerrainGenerator::queryHeight - deterministic for a given position"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm());

    for (float x = -4000.0f; x <= 4000.0f; x += 987.0f) {
        for (float z = -4000.0f; z <= 4000.0f; z += 1021.0f) {
            float h1 = generator.queryHeight(x, z);
            float h2 = generator.queryHeight(x, z);
            CHECK(h1 == h2);
        }
    }
}

TEST_CASE("TerrainGenerator::queryHeight - all 10 real classic-planet files produce finite "
          "heights near map center"
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
        auto generator = TerrainGenerator::parse(trn.generatorForm());

        for (float x = -1000.0f; x <= 1000.0f; x += 250.0f) {
            for (float z = -1000.0f; z <= 1000.0f; z += 250.0f) {
                float h = generator.queryHeight(x, z);
                CHECK(std::isfinite(h));
            }
        }
    }
}

TEST_CASE("TerrainGenerator::queryHeight - adjacent samples are smooth near map center"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm());

    // A 1-meter step shouldn't produce a huge height jump almost anywhere -
    // generous tolerance to tolerate the rare cliff/structure-adjacent
    // sample, matching this project's usual "weaker bar where no oracle
    // exists yet" caveat.
    int largeJumps = 0;
    int totalSamples = 0;
    for (float x = -2000.0f; x <= 2000.0f; x += 173.0f) {
        for (float z = -2000.0f; z <= 2000.0f; z += 191.0f) {
            float h0 = generator.queryHeight(x, z);
            float h1 = generator.queryHeight(x + 1.0f, z);
            ++totalSamples;
            if (std::fabs(h1 - h0) > 50.0f) {
                ++largeJumps;
            }
        }
    }
    CHECK(largeJumps < totalSamples / 10);
}

TEST_CASE("TerrainGenerator::queryHeight - real (x,z)->height samples from a live capture"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);

    // Real CmdStartScene samples, captured via tshark + pcapdecoder in the
    // same session against two independent real servers (a private dev
    // server and Finalizer), decoded after fixing the field-semantics bug
    // documented above. Provenance kept inline since these are the actual
    // load-bearing ground truth for this test, not illustrative examples.
    struct RealSample {
        const char* label;
        const char* planet;
        float x, z, expectedHeight, tolerance;
    };
    const RealSample samples[] = {
        // Finalizer, user-confirmed standing on open outdoor terrain.
        {"Finalizer/tatooine", "tatooine", 3250.51f, -5080.8f, 5.47721f, 2.0f},
        // Private dev server ("Naritus"), relogged outdoors after an
        // initial indoor/cell sample (kept below as a negative control) -
        // computed height matched to within 0.001m.
        {"local server/naboo (outdoor)", "naboo", -4525.0f, 2539.44f, 14.4214f, 2.0f},
    };

    for (const auto& s : samples) {
        INFO("sample: ", s.label);
        auto bytes = archive.extract(std::string("terrain/") + s.planet + ".trn");
        auto trn = TrnFile::parse(bytes);
        auto generator = TerrainGenerator::parse(trn.generatorForm());
        float computed = generator.queryHeight(s.x, s.z);
        CHECK(std::fabs(computed - s.expectedHeight) < s.tolerance);
    }
}

TEST_CASE("TerrainGenerator::queryHeight - negative control: an indoor/cell-relative sample "
          "does NOT match (proves the test above isn't vacuous)"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    // Same live session, same character, captured before relogging outside
    // (see the "outdoor" sample above) - the user confirmed this position
    // was inside a structure, so its CmdStartScene coordinates are
    // cell-relative, not world-absolute. queryHeight() only ever computes
    // world-absolute open-terrain height, so a real mismatch here is
    // expected and correct, not a bug - kept specifically to demonstrate
    // that the matching samples above aren't a coincidence of a
    // degenerate/always-close queryHeight() implementation.
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);
    auto generator = TerrainGenerator::parse(trn.generatorForm());

    float computed = generator.queryHeight(-4529.01f, 2506.1f);
    float expectedIfOutdoor = 17.4471f;
    CHECK(std::fabs(computed - expectedIfOutdoor) > 2.0f);
}
