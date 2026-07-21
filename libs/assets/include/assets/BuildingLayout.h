#pragma once

#include <string>
#include <vector>

namespace assets {

// One room/cell of a building's interior, as described by a real ".pob"
// file. Real, confirmed-live convention: cell 0 is always the building's
// own EXTERIOR shell (its `modelFilename` is the only one referencing a
// ".lod" file rather than a plain ".msh" - matches how every other outdoor
// object's highest-detail mesh is selected via LodFile), and every
// following cell is a real named interior room (confirmed against "Eese's
// House": "entry", "foyer", "halla"/"hallb", two stairwells, two closets,
// "elevator", "mainhall", "dining", "basement", four "meeting" rooms) -
// this struct doesn't encode that convention specially though; callers just
// resolve every cell's `modelFilename` uniformly (handling the real
// ".lod"-vs-direct-".msh" branch generically, exactly like
// RealMeshResolver already does for ordinary objects) and render the whole
// list.
struct BuildingCell {
    std::string name;          // e.g. "entry", "foyer" - not used for rendering, diagnostic only
    std::string modelFilename; // full archive path - a real ".msh", or ".lod" for cell 0 (exterior)
};

// Plain, renderer-agnostic data from a real ".pob" file (FORM PRTO - "Portal
// Object"). Real structure confirmed by dumping an actual extracted .pob
// (`appearance/ply_all_assoc_hall_civ_s01.pob`, "Eese's House"):
// FORM(PRTO)/FORM(0003)/CHNK(DATA) (numPortals uint32, numCells uint32) ->
// FORM(PRTS) (real portal-opening geometry - not read here, not needed to
// render a cell's own mesh) -> FORM(CELS) (numCells real FORM(CELL) blocks,
// each FORM(CELL)/FORM(000N)/CHNK(DATA): numCellPortals uint32 (unread),
// one unread byte, a NUL-terminated cell name, a NUL-terminated model
// filename, a hasFloor byte, and - only if hasFloor - a NUL-terminated
// floor (collision) filename, unread here) -> FORM(PGRF) (a real
// pathfinding graph over the cells - AI/navigation data, not needed for
// rendering) -> CHNK(CRC) (checksum, unread). Each CELL block also embeds
// real inline portal-placement/lighting/simplified-collision-mesh data
// (FORM PRTL/CHNK LGHT/FORM CMSH->FORM IDTL->CHNK VERT+INDX) that this
// parser deliberately does not read - matches this project's established
// "dead tag, don't fail on it" precedent (BLTS/HPTS/occlusion-zone chunks
// in SkeletalMesh.cpp): this data is real and useful for future
// cell-relative-movement/portal-culling work, just not for rendering a
// cell's own already-referenced mesh.
struct BuildingLayoutData {
    std::vector<BuildingCell> cells;
};

class BuildingLayout {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected.
    static BuildingLayoutData parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
