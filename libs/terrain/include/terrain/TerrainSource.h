#pragma once

#include "terrain/TerrainChunk.h"

namespace terrain {

// The one deliberate use of virtual polymorphism in this codebase
// (everywhere else uses std::variant, right for heterogeneous data - this
// is "a whole swappable module selected once per zone", which isn't the
// same shape). ProceduralTerrainSource (this file's sibling) is the only
// implementation right now - a future second one for later-expansion
// planets (confirmed by the user to use a genuinely different system) can
// plug in here without touching TerrainChunkManager or the renderer at
// all, only the one construction call site that picks which source a
// given zone uses.
class TerrainSource {
public:
    virtual ~TerrainSource() = default;

    virtual float queryHeight(float worldX, float worldZ) const = 0;
    virtual ChunkHeightData generateChunk(ChunkCoord coord) const = 0;
    virtual float chunkWidthInMeters() const = 0;
    virtual uint32_t tilesPerChunk() const = 0;
};

} // namespace terrain
