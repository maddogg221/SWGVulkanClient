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

} // namespace terrain
