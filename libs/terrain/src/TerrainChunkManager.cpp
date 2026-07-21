#include "terrain/TerrainChunkManager.h"

#include <cmath>
#include <cstdlib>
#include <utility>

namespace terrain {

TerrainChunkManager::TerrainChunkManager(std::shared_ptr<TerrainSource> source,
                                          int radiusInChunks)
    : source_(std::move(source)), radiusInChunks_(radiusInChunks) {}

void TerrainChunkManager::recenterAndEvict(float worldX, float worldZ) {
    float chunkWidth = source_->chunkWidthInMeters();

    ChunkCoord center;
    center.chunkX = static_cast<int32_t>(std::floor(worldX / chunkWidth));
    center.chunkZ = static_cast<int32_t>(std::floor(worldZ / chunkWidth));

    if (hasCenterChunk_ && center == lastCenterChunk_) {
        return;
    }
    lastCenterChunk_ = center;
    hasCenterChunk_ = true;

    for (auto it = loaded_.begin(); it != loaded_.end();) {
        int dx = it->first.chunkX - center.chunkX;
        int dz = it->first.chunkZ - center.chunkZ;
        if (std::abs(dx) > radiusInChunks_ || std::abs(dz) > radiusInChunks_) {
            it = loaded_.erase(it);
        } else {
            ++it;
        }
    }
}

void TerrainChunkManager::update(float worldX, float worldZ) {
    if (!source_ || source_->chunkWidthInMeters() <= 0.0f) {
        return;
    }
    recenterAndEvict(worldX, worldZ);

    for (int dz = -radiusInChunks_; dz <= radiusInChunks_; ++dz) {
        for (int dx = -radiusInChunks_; dx <= radiusInChunks_; ++dx) {
            ChunkCoord coord{lastCenterChunk_.chunkX + dx, lastCenterChunk_.chunkZ + dz};
            if (loaded_.find(coord) == loaded_.end()) {
                loaded_.emplace(coord, source_->generateChunk(coord));
            }
        }
    }
}

bool TerrainChunkManager::loadOneMissingChunk(float worldX, float worldZ) {
    if (!source_ || source_->chunkWidthInMeters() <= 0.0f) {
        return false;
    }
    recenterAndEvict(worldX, worldZ);

    for (int dz = -radiusInChunks_; dz <= radiusInChunks_; ++dz) {
        for (int dx = -radiusInChunks_; dx <= radiusInChunks_; ++dx) {
            ChunkCoord coord{lastCenterChunk_.chunkX + dx, lastCenterChunk_.chunkZ + dz};
            if (loaded_.find(coord) == loaded_.end()) {
                loaded_.emplace(coord, source_->generateChunk(coord));
                return true;
            }
        }
    }
    return false;
}

const std::unordered_map<ChunkCoord, ChunkHeightData, ChunkCoordHash>&
TerrainChunkManager::loadedChunks() const {
    return loaded_;
}

} // namespace terrain
