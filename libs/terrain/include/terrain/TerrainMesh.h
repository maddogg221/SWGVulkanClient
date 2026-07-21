#pragma once

#include <vector>

#include "assets/StaticMesh.h"
#include "terrain/TerrainChunk.h"

namespace terrain {

struct TerrainVertex {
    assets::Float3 position; // absolute world space - no per-draw model matrix needed
    assets::Float3 normal;
    assets::Float3 color; // normalized [0,1], converted from ChunkHeightData's Rgb8
};

struct TerrainMeshData {
    std::vector<TerrainVertex> vertices;
    std::vector<uint32_t> indices;
};

// Builds a standard grid mesh (2 triangles per tile quad) from a chunk's
// sampled height/color data, with normals derived via central-difference
// against neighboring samples within the same chunk (clamped at chunk
// edges, since neighboring chunks' samples aren't available to this
// function - a visible-but-minor seam-normal artifact at chunk boundaries,
// not attempted to be hidden here).
//
// Deliberately NOT the real engine's documented 8-triangle-fan-per-tile
// topology (PHASE_11_STATUS.md) - no real client mesh-construction code
// survives to port (a confirmed, permanent gap, not a search failure), and
// a standard grid mesh is visually equivalent to a fan at any reasonable
// tessellation density. An original design choice, not a fidelity
// compromise on anything actually recoverable from source.
TerrainMeshData buildChunkMesh(const ChunkHeightData& chunk);

} // namespace terrain
