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

// Phase 20 (collision) - real per-cell floor-collision filename and inline
// CMSH collision mesh, confirmed via a direct chunk-tree/byte dump this
// session: every one of Eese's House's 18 real cells has hasFloor=1 (a real
// ".flr" filename present, though that file's own format isn't parsed by
// this project yet) and a real inline CMSH mesh (VERT/INDX, no leading
// counts on either - simpler than StaticMesh's own format).
TEST_CASE("BuildingLayout: parses real per-cell collision data (floor filename + CMSH mesh)"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string pobPath = "appearance/ply_all_assoc_hall_civ_s01.pob";
    REQUIRE(archive.contains(pobPath));

    auto bytes = archive.extract(pobPath);
    auto layout = BuildingLayout::parse(bytes);
    REQUIRE(layout.cells.size() == 18);

    // Cell 0 ("r0", the exterior shell): real confirmed values from a
    // direct byte dump this session.
    const auto& r0 = layout.cells[0];
    CHECK(r0.floorCollisionFilename ==
          "appearance/collision/ply_all_assoc_hall_civ_s01_r0_collision_floor.flr");
    CHECK(r0.collisionMesh.positions.size() == 68);  // 816 real VERT bytes / 12
    CHECK(r0.collisionMesh.indices.size() == 258);   // 1032 real INDX bytes / 4 (86 triangles)

    // Cell 1 ("entry"): a second real cell, independently confirmed.
    const auto& entry = layout.cells[1];
    CHECK(entry.floorCollisionFilename ==
          "appearance/collision/ply_all_assoc_hall_civ_s01_r1_entry_collision_floor.flr");
    CHECK(entry.collisionMesh.positions.size() == 48);  // 576 real VERT bytes / 12
    CHECK(entry.collisionMesh.indices.size() == 126);   // 504 real INDX bytes / 4 (42 triangles)

    // Every real index must reference a real vertex - the same end-to-end
    // sanity check StaticMesh's own tests use.
    for (uint32_t index : entry.collisionMesh.indices) {
        CHECK(index < entry.collisionMesh.positions.size());
    }
}

// Phase 20b (portal-based cell transitions) - real building-wide portal
// SHAPE list (FORM PRTS) and real per-cell portal PLACEMENTs (FORM PRTL,
// nested one level inside each cell), confirmed via a direct byte dump
// this session: byte[1] of each 60-byte placement record is the real
// portal shape index, byte[6] is the real adjacent-cell index (see
// BuildingLayout.cpp's own readPortalPlacement() comment for the full
// byte-format writeup).
TEST_CASE("BuildingLayout: parses real portal shapes and per-cell portal placements"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string pobPath = "appearance/ply_all_assoc_hall_civ_s01.pob";
    REQUIRE(archive.contains(pobPath));

    auto bytes = archive.extract(pobPath);
    auto layout = BuildingLayout::parse(bytes);
    REQUIRE(layout.cells.size() == 18);

    // Real, confirmed: 21 real portal shapes, shared building-wide.
    REQUIRE(layout.portalShapes.size() == 21);
    CHECK(layout.portalShapes[0].vertices.size() == 6);
    CHECK(layout.portalShapes[3].vertices.size() == 4);

    // Cell 0 ("r0", the exterior shell): real confirmed 3 portals, to
    // "entry" (cell 1), "stairwella" (cell 4), and "stairwellb" (cell 6).
    const auto& r0 = layout.cells[0];
    REQUIRE(r0.portals.size() == 3);
    CHECK(r0.portals[0].portalShapeIndex == 0);
    CHECK(r0.portals[0].adjacentCellIndex == 1);
    CHECK(r0.portals[1].portalShapeIndex == 1);
    CHECK(r0.portals[1].adjacentCellIndex == 4);
    CHECK(r0.portals[2].portalShapeIndex == 2);
    CHECK(r0.portals[2].adjacentCellIndex == 6);

    // Cell 1 ("entry"): real confirmed 3 portals - back to "r0" (cell 0,
    // the reverse of r0's own first portal, sharing the same real shape
    // index 0), and TWO separate portals forward to "foyer" (cell 2, real
    // shape indices 3 and 4 - two distinct real openings between the same
    // pair of cells, not a parsing artifact).
    const auto& entry = layout.cells[1];
    REQUIRE(entry.portals.size() == 3);
    CHECK(entry.portals[0].portalShapeIndex == 0);
    CHECK(entry.portals[0].adjacentCellIndex == 0);
    CHECK(entry.portals[1].portalShapeIndex == 3);
    CHECK(entry.portals[1].adjacentCellIndex == 2);
    CHECK(entry.portals[2].portalShapeIndex == 4);
    CHECK(entry.portals[2].adjacentCellIndex == 2);

    // Every real portal placement's shape index must reference a real
    // shape, and every adjacent cell index a real cell - end-to-end sanity
    // check across all 18 cells, same spirit as the collision index check
    // above.
    for (const auto& cell : layout.cells) {
        for (const auto& portal : cell.portals) {
            CHECK(portal.portalShapeIndex < layout.portalShapes.size());
            CHECK(portal.adjacentCellIndex < layout.cells.size());
        }
    }
}

// Phase 20c (real dedicated floor-collision navmesh) - a SEPARATE real file
// from the inline CMSH data, referenced by BuildingCell::floorCollisionFilename.
// Real, confirmed via a direct byte dump this session: CHNK(VERT) has a
// real leading uint32 count (unlike CMSH's own VERT), and CHNK(TRIS) is a
// leading uint32 count followed by rich 60-byte real per-triangle records
// (only the first 3 uint32 vertex indices are read; the rest - real
// adjacency/plane-equation/boundary-edge data - is skipped).
TEST_CASE("FloorCollision: parses a real stairwell's dedicated .flr navmesh"
          * doctest::skip(!std::filesystem::exists(kOtherArchivePath))) {
    TreArchive archive(kOtherArchivePath);
    const std::string flrPath =
        "appearance/collision/ply_all_assoc_hall_civ_s01_r4_stairwella_collision_floor.flr";
    REQUIRE(archive.contains(flrPath));

    auto bytes = archive.extract(flrPath);
    auto mesh = FloorCollision::parse(bytes);

    // Real, confirmed values from a direct byte dump this session: VERT is
    // 592 bytes (4-byte count + 49 * 12-byte Float3 = 592), TRIS is 2884
    // bytes (4-byte count + 48 * 60-byte records = 2884).
    REQUIRE(mesh.positions.size() == 49);
    REQUIRE(mesh.triangleVertexIndices.size() == 144); // 48 triangles * 3

    // Every real triangle index must reference a real vertex - the same
    // end-to-end sanity check the other real mesh parsers in this project
    // use.
    for (uint32_t index : mesh.triangleVertexIndices) {
        CHECK(index < mesh.positions.size());
    }
}
