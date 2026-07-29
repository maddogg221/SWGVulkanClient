#pragma once

#include <string>
#include <vector>

#include "assets/IffReader.h"
#include "terrain/Affector.h"
#include "terrain/Boundary.h"
#include "terrain/Filter.h"
#include "terrain/LayerItem.h"

namespace terrain {

// A `FORM LAYR` - the tree node the whole generator is built from. Real
// classic-planet files exclusively use LAYR version 0003 (confirmed: all
// 2054 real layers across all 10 classic .trn files, zero exceptions).
// Child Boundary/Filter/Affector/nested-Layer forms are dispatched purely
// by IFF FORM tag, matching the real engine's own dispatch order (boundary
// tags tried first, then filter, then affector, then it must be a nested
// `FORM LAYR` or the file is malformed).
struct Layer {
    LayerItemHeader header;
    bool invertBoundaries = false;
    bool invertFilters = false;
    bool expanded = false;
    std::string notes;
    std::vector<Boundary> boundaries;
    std::vector<Filter> filters;
    std::vector<Affector> affectors;
    std::vector<Layer> subLayers;
};

// `layrForm` is a `FORM LAYR` node. Throws std::runtime_error for an
// unsupported version or an unrecognized child tag.
Layer parseLayer(const assets::IffChunk& layrForm);

// Recursively shifts every Boundary in `layer` (and its subLayers) by
// (dx, dz) in world space - ports Core3's ProceduralTerrainAppearance::
// translateBoundary() exactly. Used to move a standalone `.lay`
// terrain-modification file's own layer tree (authored in a local/relative
// coordinate space) to a building's real world placement position.
void translateLayerBoundaries(Layer& layer, float dx, float dz);

// Recursively sets AffectorHeightConstant::height = height for every
// affector in `layer` (and its subLayers) where operation == TGO_replace(0)
// && height == 0.0f - i.e. only the placeholder flatten-to-height affectors
// a `.lay` file's author left unset, never an already-authored non-zero
// height. Ports Core3's ProceduralTerrainAppearance::setHeight() guard
// exactly. Used to bake in the terrain height sampled at a building's
// placement point BEFORE the modification is applied, so a structure always
// grades to whatever height was already there, not a fixed global height.
void bakeLayerHeight(Layer& layer, float height);

} // namespace terrain
