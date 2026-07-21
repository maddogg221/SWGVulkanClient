#pragma once

#include <cmath>

#include "assets/StaticMesh.h"

namespace terrain {

// Central-difference surface normal from 4 height samples straddling a
// point at `sampleSpacing` apart (2 samples per axis, e.g. height(x-d,z)
// and height(x+d,z)) - shared by TerrainGenerator's FilterSlope/
// FilterDirection support (sampled at generation time) and
// TerrainMesh::buildChunkMesh() (sampled at mesh-build time from the
// already-baked heightfield). Falls back to a straight-up normal if the
// 4 samples are degenerate (zero-length result), which only happens on a
// perfectly flat, zero-height surface.
inline assets::Float3 normalFromHeightSamples(float heightLeft, float heightRight,
                                               float heightDown, float heightUp,
                                               float sampleSpacing) {
    assets::Float3 normal;
    normal.x = heightLeft - heightRight;
    normal.y = 2.0f * sampleSpacing;
    normal.z = heightDown - heightUp;
    float len = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    if (len > 0.0f) {
        normal.x /= len;
        normal.y /= len;
        normal.z /= len;
    }
    return normal;
}

} // namespace terrain
