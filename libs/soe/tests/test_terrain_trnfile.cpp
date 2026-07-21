// Permanent test for terrain::TrnFile - the .trn container/header parser.
// Requires the real client install to be present on this machine (same
// archive libs/assets' own tests already gate on) - skipped, not failed,
// wherever it isn't. See PHASE_11_STATUS.md for the full research trace
// this parser was built from.
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/TreArchive.h"
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
} // namespace

TEST_CASE("TrnFile::parse - all 10 real classic-planet .trn files parse without throwing"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    for (const char* name : kAllPlanetFiles) {
        INFO("planet file: ", name);
        REQUIRE(archive.contains(name));
        auto bytes = archive.extract(name);
        auto trn = TrnFile::parse(bytes);

        CHECK(trn.header().mapWidthInMeters > 0.0f);
        CHECK(trn.header().chunkWidthInMeters > 0.0f);
        CHECK(trn.header().numberOfTilesPerChunk > 0);
        CHECK(trn.header().tileWidthInMeters() > 0.0f);
        CHECK(trn.generatorForm().id == assets::kFormTag);
    }
}

TEST_CASE("TrnFile::parse - naboo.trn real header field values"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    auto bytes = archive.extract("terrain/naboo.trn");
    auto trn = TrnFile::parse(bytes);

    const auto& header = trn.header();
    // Real values dumped from this exact file this session (see
    // PHASE_11_STATUS.md) - pinned rather than just sanity-checked, per the
    // terrain plan's phase 1 verification bar. name is the literal build
    // path baked into the file by the original tool, not a runtime path on
    // this machine. chunkWidthInMeters=8 (only 2 tiles/chunk) is a real,
    // confirmed value - smaller than the illustrative "32m chunks" example
    // mentioned during Phase 11 research; not yet reconciled against the
    // real quadtree's own chunk-space sizing (open item for phase 6).
    CHECK(header.name == "C:\\SWO\\swg\\current\\data\\sku.0\\sys.shared\\built\\game\\terrain\\naboo.trn");
    CHECK(header.mapWidthInMeters == doctest::Approx(16384.0f));
    CHECK(header.chunkWidthInMeters == doctest::Approx(8.0f));
    CHECK(header.numberOfTilesPerChunk == 2);
    CHECK(header.tileWidthInMeters() == doctest::Approx(4.0f));
    CHECK(header.useGlobalWaterTable == true);
    CHECK(header.globalWaterTableHeight == doctest::Approx(-210.0f));
    CHECK(header.environmentCycleTime == doctest::Approx(10620.0f));
    CHECK(header.collidable.seed == 0);
    CHECK(header.legacyMap == false);

    CHECK(trn.generatorForm().formType != 0);
}
