#include "terrain/TerrainMesh.h"

#include <algorithm>
#include <cmath>

#include "NormalMath.h"

namespace terrain {

namespace {

float heightAt(const ChunkHeightData& chunk, int x, int z) {
    int n = static_cast<int>(chunk.verticesPerSide);
    x = std::clamp(x, 0, n - 1);
    z = std::clamp(z, 0, n - 1);
    return chunk.heights[static_cast<size_t>(z) * chunk.verticesPerSide + static_cast<size_t>(x)];
}

} // namespace

TerrainMeshData buildChunkMesh(const ChunkHeightData& chunk) {
    TerrainMeshData mesh;
    uint32_t n = chunk.verticesPerSide;
    mesh.vertices.reserve(static_cast<size_t>(n) * n);

    for (uint32_t z = 0; z < n; ++z) {
        for (uint32_t x = 0; x < n; ++x) {
            TerrainVertex v;
            float worldX = chunk.worldOriginX + static_cast<float>(x) * chunk.tileWidthInMeters;
            float worldZ = chunk.worldOriginZ + static_cast<float>(z) * chunk.tileWidthInMeters;
            float h = heightAt(chunk, static_cast<int>(x), static_cast<int>(z));
            v.position = {worldX, h, worldZ};

            float hL = heightAt(chunk, static_cast<int>(x) - 1, static_cast<int>(z));
            float hR = heightAt(chunk, static_cast<int>(x) + 1, static_cast<int>(z));
            float hD = heightAt(chunk, static_cast<int>(x), static_cast<int>(z) - 1);
            float hU = heightAt(chunk, static_cast<int>(x), static_cast<int>(z) + 1);
            v.normal = normalFromHeightSamples(hL, hR, hD, hU, chunk.tileWidthInMeters);

            const assets::Rgb8& c =
                chunk.colors[static_cast<size_t>(z) * n + static_cast<size_t>(x)];
            v.color = {static_cast<float>(c.r) / 255.0f, static_cast<float>(c.g) / 255.0f,
                       static_cast<float>(c.b) / 255.0f};

            mesh.vertices.push_back(v);
        }
    }

    // Two triangles per quad. Winding doesn't affect visibility here - the
    // terrain pipeline uses cull mode NONE (matching drawMesh()'s own
    // existing precedent in VulkanRenderer.cpp), so an unexpected winding
    // would only ever show up as inverted-looking lighting, never missing
    // geometry.
    for (uint32_t z = 0; z + 1 < n; ++z) {
        for (uint32_t x = 0; x + 1 < n; ++x) {
            uint32_t i00 = z * n + x;
            uint32_t i10 = z * n + x + 1;
            uint32_t i01 = (z + 1) * n + x;
            uint32_t i11 = (z + 1) * n + x + 1;
            mesh.indices.push_back(i00);
            mesh.indices.push_back(i01);
            mesh.indices.push_back(i10);
            mesh.indices.push_back(i10);
            mesh.indices.push_back(i01);
            mesh.indices.push_back(i11);
        }
    }

    return mesh;
}

} // namespace terrain
