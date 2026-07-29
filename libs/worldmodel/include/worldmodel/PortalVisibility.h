#pragma once

#include <cstddef>
#include <functional>
#include <vector>

#include "assets/BuildingLayout.h"

namespace worldmodel {

// Real portal-based cell visibility: starting from `startCellIndex` (always
// marked visible - typically the viewer's own current cell, or cell 0/the
// exterior shell when the viewer is outside the building entirely), walks
// the real per-cell portal adjacency graph (`cellPortals[i]` - each cell's
// own `assets::CellPortal` list, from a real parsed `.pob` file's
// `assets::BuildingCell::portals`, or an equivalent per-cell list a caller
// already carries - see PortalShape/CellPortal's own comments in
// BuildingLayout.h for the real format) breadth-first. For each candidate
// portal, its real shape is approximated as a bounding sphere (centroid +
// max vertex distance from it - the same approximation this project's own
// segmentCrossesPortal() portal-CROSSING test already uses for door-sized
// SWG portals, just reused here for a visibility test instead) and tested
// via `isSphereVisible` before recursing through it. Bounded by `maxDepth`
// as a guard against a degenerate/cyclic portal graph, not a real gameplay
// constraint - real SWG interiors are shallow, so the default is generous
// and should never visibly clip a real building.
//
// Pure function: no rendering/threading/platform dependency, so this stays
// unit-testable with hand-built fixtures and worldmodel itself never needs
// to depend on the renderer library. Deliberately takes just the per-cell
// portal lists (not a full assets::BuildingCell/ResolvedCell) since that's
// the only real data the algorithm needs - callers holding either type can
// pass its `.portals` field directly. The caller supplies the actual
// frustum/camera test via `isSphereVisible` (e.g. wrapping
// renderer::Frustum::intersectsSphere()). `center` passed to that callback
// is in the SAME coordinate space `cellPortals`/`portalShapes` themselves
// use (real `.pob` portal vertices are building-local, not world) - a
// caller needing a world-space frustum test is responsible for
// transforming inside its own callback (e.g. via a
// `buildingLocalToWorld()` transform), not this function.
//
// Returns a bool per entry in `cellPortals` (same indexing), true if that
// cell is visible from `startCellIndex` this call.
// `startCellIndex >= cellPortals.size()` returns an all-false vector
// (nothing to walk from).
std::vector<bool> computeVisibleCells(
    const std::vector<std::vector<assets::CellPortal>>& cellPortals,
    const std::vector<assets::PortalShape>& portalShapes, size_t startCellIndex,
    const std::function<bool(assets::Float3 center, float radius)>& isSphereVisible, int maxDepth = 8);

} // namespace worldmodel
