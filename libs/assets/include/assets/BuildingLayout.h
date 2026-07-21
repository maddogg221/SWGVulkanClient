#pragma once

#include <string>
#include <vector>

#include "assets/StaticMesh.h" // MeshData, reused for real collision geometry

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
// A real portal opening's own polygon shape (Phase 20b), from the
// building-wide FORM(PRTS) list - a flat, planar, real convex polygon
// (vertex winding not independently confirmed, but not needed for a plane/
// bounding-sphere crossing test). Indexed positionally by
// BuildingCell::portals[].portalShapeIndex; index alignment with FORM(PRTS)'s
// own child order is preserved even for a malformed shape (pushed empty)
// so that indexing never goes stale.
struct PortalShape {
    std::vector<Float3> vertices;
};

// One of a cell's own real portal placements - see BuildingCell::portals'
// own comment for the full real byte-format writeup.
struct CellPortal {
    uint32_t portalShapeIndex = 0;
    uint32_t adjacentCellIndex = 0;
};

struct BuildingCell {
    std::string name;          // e.g. "entry", "foyer" - not used for rendering, diagnostic only
    std::string modelFilename; // full archive path - a real ".msh", or ".lod" for cell 0 (exterior)

    // Real collision data (Phase 20). floorCollisionFilename is the real
    // per-cell ".flr" path (only present when a cell's own `hasFloor` byte
    // is set - true for all 18 real cells checked so far) - the file itself
    // is a real, more specialized format (its own VERT/TRIS mesh plus a
    // pre-built BTRE acceleration tree and real pathfinding-graph data,
    // confirmed via a direct chunk-tree dump) not yet parsed by this
    // project; captured here for future use, empty string if absent.
    // collisionMesh is the real INLINE per-cell collision mesh (FORM CMSH ->
    // FORM IDTL -> CHNK VERT+INDX) this parser DOES read - real, confirmed
    // simple format: VERT is a flat, unprefixed array of Float3 positions
    // (byte size always divides evenly by 12 in every real cell checked),
    // INDX is a flat, unprefixed array of uint32 triangle indices (always
    // divides evenly by 4, and further by 3 for whole triangles) - notably
    // NEITHER carries a leading count field, unlike StaticMesh's own INDX
    // chunk. Only `positions`/`indices` are populated (no normals/UV0 -
    // this is collision-only geometry, not visual). Empty (zero positions)
    // if the real cell has no CMSH data, or if it failed to parse - callers
    // must treat that as "no collision geometry for this cell" and fall
    // back gracefully (e.g. no wall blocking / no floor-height query for
    // that cell), never a hard failure - matches this project's established
    // per-part skip/fallback convention.
    std::string floorCollisionFilename;
    MeshData collisionMesh;

    // Real portal references (Phase 20b - cell-transition research). Each
    // entry is one of this cell's own real FORM(PRTL) placements (count
    // matches the cell's own numCellPortals field, read but previously
    // unused) - portalShapeIndex indexes into BuildingLayoutData::
    // portalShapes (the real, shared, building-wide flat list of portal
    // polygons), adjacentCellIndex is which other real cell this specific
    // portal leads to. Confirmed via a direct byte dump this session: a
    // building-wide FORM(PRTS) holds the real polygon SHAPES once each
    // (flat, unprefixed Float3 lists, leading uint32 vertex count), while
    // each cell's own FORM(PRTL) placement is a small fixed 60-byte record
    // (CHNK "0004") - byte[1] the shape index, byte[6] the adjacent cell
    // index, followed by a 3x3 rotation + Float3 translation that was
    // identity/zero in every real portal checked (consistent with this
    // project's established fact that every cell already shares one common
    // building-local coordinate frame - see worldToBuildingLocal's own
    // comment in Visualizer.cpp). Two unidentified single-byte fields
    // (always 1, and a 0/1 flag that flips between the two cells sharing a
    // portal, maybe orientation/winding) are read but not used - see
    // BuildingLayout.cpp's own comment.
    std::vector<CellPortal> portals;
};

// Plain, renderer-agnostic data from a real ".pob" file (FORM PRTO - "Portal
// Object"). Real structure confirmed by dumping an actual extracted .pob
// (`appearance/ply_all_assoc_hall_civ_s01.pob`, "Eese's House"):
// FORM(PRTO)/FORM(0003)/CHNK(DATA) (numPortals uint32, numCells uint32) ->
// FORM(PRTS) (real portal-opening SHAPES, a flat list shared building-wide -
// now read into portalShapes, see PortalShape's own comment) -> FORM(CELS)
// (numCells real FORM(CELL) blocks, each FORM(CELL)/FORM(000N)/CHNK(DATA):
// numCellPortals uint32 (unread, but its count matches the real number of
// FORM(PRTL) placements now read per cell), one unread byte, a
// NUL-terminated cell name, a NUL-terminated model filename, a hasFloor
// byte, and - only if hasFloor - a NUL-terminated floor (collision)
// filename, now read into BuildingCell::floorCollisionFilename, the file
// itself still unparsed) -> FORM(PGRF) (a real pathfinding graph over the
// cells - AI/navigation data, not needed here) -> CHNK(CRC) (checksum,
// unread). Each CELL block also embeds real inline portal-placement/
// lighting/collision-mesh data (FORM PRTL/CHNK LGHT/FORM CMSH->FORM IDTL->
// CHNK VERT+INDX) - CMSH is read into BuildingCell::collisionMesh, PRTL is
// now read into BuildingCell::portals (see its own comment); LGHT remains
// unread, matching this project's established "dead tag, don't fail on it"
// precedent (BLTS/HPTS/occlusion-zone chunks in SkeletalMesh.cpp).
struct BuildingLayoutData {
    std::vector<BuildingCell> cells;
    std::vector<PortalShape> portalShapes;
};

class BuildingLayout {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected.
    static BuildingLayoutData parse(const std::vector<uint8_t>& bytes);
};

// Real, dedicated per-cell floor-collision navmesh (Phase 20c), a SEPARATE
// real file from the inline CMSH data (see BuildingCell::floorCollisionFilename's
// own comment) - real, confirmed format via a direct byte dump this
// session: FORM(FLOR)->FORM(0006)->CHNK(VERT) (real, leading uint32 count +
// that many Float3 positions - unlike CMSH's own VERT chunk, this one DOES
// carry a leading count) + CHNK(TRIS) (a leading uint32 count + that many
// rich 60-byte real per-triangle records: 3 vertex indices, then real
// triangle-adjacency/plane-equation/boundary-edge data this project doesn't
// need and doesn't read) + FORM(BTRE) (a pre-built spatial acceleration
// tree over the triangles - real, unread, not needed since this building's
// real floor meshes are small enough for a plain per-frame linear scan) +
// CHNK(BEDG) (boundary-edge data, unread) + FORM(PGRF) (a real per-cell
// pathfinding graph - AI/navigation data, unread, unrelated to player
// collision). Only positions + the flat triangleVertexIndices list are
// read; everything else in the file is real, confirmed, and deliberately
// left unread, matching this project's established "dead tag, don't fail
// on it" precedent.
//
// Why this exists alongside CMSH: CMSH's own single-vertical-raycast
// height query is fundamentally ambiguous on a real switchback staircase
// (two flights stacked over one another at the same X/Z - confirmed live,
// this project's own Phase 20b/c debugging). A real 2D (X/Z-projected)
// point-in-triangle test against this file's own dedicated floor mesh
// instead only matches the ONE tread actually underfoot, since (in a real,
// non-self-overlapping floor mesh) at most one triangle's own real 2D
// footprint contains any given X/Z point - no vertical-stacking ambiguity
// to begin with.
struct FloorCollisionMesh {
    std::vector<Float3> positions;
    std::vector<uint32_t> triangleVertexIndices; // flat, 3 per real triangle
    // Real per-triangle adjacency (Phase 20c continued) - flat, 3 per real
    // triangle, one per edge (v0-v1, v1-v2, v2-v0) - the real NEIGHBORING
    // triangle index across that edge, or -1 (no neighbor - a real mesh
    // boundary edge). Confirmed via a direct byte dump this session: two
    // triangles sharing a real edge cross-reference each other's index
    // here. Needed because a real switchback staircase's lower flight can
    // share the SAME X/Z footprint (viewed from directly above) as its own
    // upper entrance - a plain "does any triangle's 2D footprint contain
    // this point" query is genuinely ambiguous there. Walking only to a
    // real ADJACENT triangle each frame (see Visualizer.cpp's own
    // adjacency-walk comment) is the correct fix: it's a continuity
    // constraint, not a height heuristic, so it can't be fooled by two
    // unconnected surfaces that merely happen to overlap in 2D.
    std::vector<int32_t> triangleNeighbors;
};

class FloorCollision {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected.
    static FloorCollisionMesh parse(const std::vector<uint8_t>& bytes);
};

} // namespace assets
