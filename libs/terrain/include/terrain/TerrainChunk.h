#pragma once

#include <cstdint>
#include <vector>

#include "assets/StaticMesh.h"
#include "assets/TgaImage.h"

namespace terrain {

// Identifies one streaming/rendering chunk - NOT the same granularity as
// the .trn file's own internal "chunk" concept (see ProceduralTerrainSource.h
// for why those are deliberately decoupled).
struct ChunkCoord {
    int32_t chunkX = 0;
    int32_t chunkZ = 0;
    bool operator==(const ChunkCoord&) const = default;
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const noexcept {
        return (static_cast<size_t>(static_cast<uint32_t>(c.chunkX)) << 32) |
               static_cast<uint32_t>(c.chunkZ);
    }
};

// Source-agnostic sampled height/color data for one chunk - no Layer/
// Affector/procedural-generation type ever crosses this boundary, so a
// future second TerrainSource (later-expansion planets, a different
// system entirely per this project's own research) only ever needs to
// produce this same struct to plug into TerrainChunkManager/mesh-building
// unchanged.
struct ChunkHeightData {
    ChunkCoord coord;
    uint32_t verticesPerSide = 0; // tilesPerChunk + 1
    float worldOriginX = 0.0f;
    float worldOriginZ = 0.0f;
    float tileWidthInMeters = 0.0f;
    std::vector<float> heights;        // row-major (z-major), verticesPerSide^2
    std::vector<assets::Rgb8> colors;  // same layout
};

} // namespace terrain
