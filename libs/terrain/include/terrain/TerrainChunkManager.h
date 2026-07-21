#pragma once

#include <memory>
#include <unordered_map>

#include "terrain/TerrainChunk.h"
#include "terrain/TerrainSource.h"

namespace terrain {

// Deliberately simplified first pass: fixed radius, single LOD, no
// quadtree, no neighbor-linked seam handling. Rendering-agnostic - only
// ever hands out ChunkHeightData; turning that into GPU geometry is the
// renderer/visualizer's job. Visible seams between chunks are a known
// limitation, not hidden (see TerrainMesh.h's own normal-clamping note).
//
// Taking `std::shared_ptr<TerrainSource>` (not ProceduralTerrainSource
// directly) IS the extensibility seam in practice - this class never
// changes when a second TerrainSource exists; only the one construction
// call site (choosing which source a given zone uses) grows a
// planet-name-keyed factory.
class TerrainChunkManager {
public:
    explicit TerrainChunkManager(std::shared_ptr<TerrainSource> source, int radiusInChunks = 3);

    // Loads newly-in-range chunks and evicts out-of-range ones around
    // (worldX, worldZ). Cheap to call every frame - a no-op once the
    // center hasn't crossed into a new chunk since the last call. Real
    // per-chunk generation cost (`TerrainSource::generateChunk()`,
    // confirmed ~126ms/chunk in a Release build - see Phase 14's own
    // notes) happens synchronously, for EVERY chunk newly in range, in one
    // call - up to `(2*radiusInChunks+1)^2` chunks the first time a
    // caller's position falls in a new center chunk. Callers that can't
    // afford that (e.g. a shared worker-thread loop that also needs to
    // service other queues) should use loadOneMissingChunk() instead.
    void update(float worldX, float worldZ);

    // Same recenter/evict bookkeeping as update(), but generates AT MOST
    // ONE missing chunk per call - lets a caller spread the real
    // generation cost across multiple calls (e.g. once per worker-thread
    // loop iteration, matching the same per-frame budget philosophy this
    // project already uses for GPU asset uploads - see AssetWorkerThread/
    // kMaxAssetUploadsPerFrame in Visualizer.cpp) instead of blocking on
    // the whole radius at once like update() does. Returns true if a chunk
    // was generated this call, false if every in-range chunk is already
    // loaded (nothing to do this call). Added 2026-07-19 (Phase 15) after
    // a live test showed update()'s all-at-once cost, worse in a Debug
    // build than the Release-measured figure above, starving the same
    // worker thread's mesh/skeletal-mesh job queues for the whole initial
    // terrain burst.
    bool loadOneMissingChunk(float worldX, float worldZ);

    const std::unordered_map<ChunkCoord, ChunkHeightData, ChunkCoordHash>& loadedChunks() const;

private:
    // Recomputes the center chunk from (worldX, worldZ) and evicts any
    // loaded chunk now outside radiusInChunks_ of it - a no-op if the
    // center hasn't changed since the last call. Shared by update() and
    // loadOneMissingChunk(), which differ only in how much GENERATION work
    // they do after this bookkeeping.
    void recenterAndEvict(float worldX, float worldZ);

    std::shared_ptr<TerrainSource> source_;
    int radiusInChunks_;
    ChunkCoord lastCenterChunk_{};
    bool hasCenterChunk_ = false;
    std::unordered_map<ChunkCoord, ChunkHeightData, ChunkCoordHash> loaded_;
};

} // namespace terrain
