// Permanent tests for assets::BuildingLayout against a real client .pob
// file, gated on the real client install being present (same pattern as
// test_assets_lodfile.cpp's live-archive test).
#include <doctest/doctest.h>

#include <filesystem>

#include "assets/BuildingLayout.h"
#include "assets/SharedObjectTemplate.h"
#include "assets/TreArchive.h"

using namespace assets;

namespace {
const char* kOtherArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
}

TEST_CASE("SharedObjectTemplate: a real building leaves appearanceFilename empty and sets "
          "portalLayoutFilename instead"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string iffPath = "object/building/player/shared_player_guildhall_generic_style_01.iff";
    REQUIRE(archive.contains(iffPath));

    auto bytes = archive.extract(iffPath);
    auto tmpl = SharedObjectTemplate::parse(bytes);
    CHECK(tmpl.appearanceFilename.empty());
    CHECK(tmpl.portalLayoutFilename == "appearance/ply_all_assoc_hall_civ_s01.pob");
}

TEST_CASE("BuildingLayout: parses real cells (exterior + interior rooms) from a real .pob file"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string pobPath = "appearance/ply_all_assoc_hall_civ_s01.pob";
    REQUIRE(archive.contains(pobPath));

    auto bytes = archive.extract(pobPath);
    auto layout = BuildingLayout::parse(bytes);

    REQUIRE(layout.cells.size() == 18);

    // Cell 0 is always the exterior shell - real, confirmed convention:
    // it's the only cell whose model is a ".lod" (LOD-selector) reference,
    // every other cell is a direct ".msh".
    CHECK(layout.cells[0].name == "r0");
    CHECK(layout.cells[0].modelFilename == "appearance/lod/ply_all_assoc_hall_civ_s01_r0_mesh.lod");

    CHECK(layout.cells[1].name == "entry");
    CHECK(layout.cells[1].modelFilename ==
          "appearance/mesh/ply_all_assoc_hall_civ_s01_r1_entry_mesh_entry.msh");

    // Every cell after the exterior is a direct .msh, never a .lod.
    for (size_t i = 1; i < layout.cells.size(); ++i) {
        const auto& filename = layout.cells[i].modelFilename;
        CHECK(filename.size() >= 4);
        CHECK(filename.compare(filename.size() - 4, 4, ".msh") == 0);
    }
}
