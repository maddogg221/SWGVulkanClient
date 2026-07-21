#include "terrain/ProceduralTerrainSource.h"

namespace terrain {

ProceduralTerrainSource ProceduralTerrainSource::parse(const std::vector<uint8_t>& bytes,
                                                         const assets::TreArchive* textureArchive) {
    TrnFile trn = TrnFile::parse(bytes);
    ProceduralTerrainSource source;
    source.header_ = trn.header();
    source.generator_ = TerrainGenerator::parse(trn.generatorForm(), textureArchive);
    return source;
}

float ProceduralTerrainSource::queryHeight(float worldX, float worldZ) const {
    return generator_.queryHeight(worldX, worldZ);
}

float ProceduralTerrainSource::chunkWidthInMeters() const {
    return kChunkWidthInMeters;
}

uint32_t ProceduralTerrainSource::tilesPerChunk() const {
    return kTilesPerChunk;
}

ChunkHeightData ProceduralTerrainSource::generateChunk(ChunkCoord coord) const {
    ChunkHeightData data;
    data.coord = coord;
    data.verticesPerSide = kTilesPerChunk + 1;
    data.tileWidthInMeters = kChunkWidthInMeters / static_cast<float>(kTilesPerChunk);

    float chunkWidth = chunkWidthInMeters();
    data.worldOriginX = static_cast<float>(coord.chunkX) * chunkWidth;
    data.worldOriginZ = static_cast<float>(coord.chunkZ) * chunkWidth;

    uint32_t n = data.verticesPerSide;
    data.heights.reserve(static_cast<size_t>(n) * n);
    data.colors.reserve(static_cast<size_t>(n) * n);
    for (uint32_t z = 0; z < n; ++z) {
        for (uint32_t x = 0; x < n; ++x) {
            float worldX = data.worldOriginX + static_cast<float>(x) * data.tileWidthInMeters;
            float worldZ = data.worldOriginZ + static_cast<float>(z) * data.tileWidthInMeters;
            // One combined walk instead of separate queryHeight()+queryColor()
            // calls - halves real, measured per-chunk generation cost.
            TerrainGenerator::Point p = generator_.queryPoint(worldX, worldZ);
            data.heights.push_back(p.height);
            data.colors.push_back(p.color);
        }
    }
    return data;
}

} // namespace terrain
