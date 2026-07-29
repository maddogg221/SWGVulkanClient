#pragma once

#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <utility>
#include <vector>

#include "assets/TreArchive.h"
#include "terrain/TerrainGenerator.h"
#include "terrain/TerrainSource.h"
#include "terrain/TrnFile.h"

namespace terrain {

// The classic procedural terrain system (TrnFile + TerrainGenerator),
// wrapped behind TerrainSource. This is the concrete implementation this
// whole terrain plan has been building toward - every real classic-planet
// .trn file this project's real test servers actually serve.
class ProceduralTerrainSource final : public TerrainSource {
public:
    // Rendering/streaming chunk world size and per-chunk tessellation -
    // deliberately NOT the .trn file's own internal chunkWidthInMeters/
    // numberOfTilesPerChunk/tileWidthInMeters (confirmed this session to be
    // unexpectedly fine-grained per real file, e.g. naboo's real values
    // give a 4m real tile width - querying at that density is real,
    // measured, prohibitively expensive: ~950ms to generate a single
    // 33x33-vertex chunk in a Release build, since every query point walks
    // the file's full Boundary/Filter/Affector layer tree from scratch).
    // Real per-planet chunk/LOD sizing is data-driven and not recoverable
    // from source anyway (PHASE_11_STATUS.md) - these are original design
    // choices, not a port, deliberately coarser than the real file's own
    // tile density to keep chunk generation practical for live use before
    // any real query-side optimization (e.g. a per-layer bounding-box
    // early-out) exists - flagged as follow-up work, not attempted here
    // under this session's own time constraints. queryHeight()/queryColor()
    // themselves are pure per-point functions, unaffected by this choice.
    static constexpr float kChunkWidthInMeters = 128.0f;
    static constexpr uint32_t kTilesPerChunk = 16;

    // Throws std::runtime_error for anything TrnFile::parse()/
    // TerrainGenerator::parse() would throw for. `textureArchive`, if
    // supplied, is used to resolve real color-ramp images - see
    // TerrainGenerator::parse()'s own comment.
    static ProceduralTerrainSource parse(const std::vector<uint8_t>& bytes,
                                          const assets::TreArchive* textureArchive = nullptr);

    float queryHeight(float worldX, float worldZ) const override;
    ChunkHeightData generateChunk(ChunkCoord coord) const override;
    float chunkWidthInMeters() const override;
    uint32_t tilesPerChunk() const override;

    // Registers/replaces a per-building terrain-modification generator
    // (e.g. TerrainGenerator::parseStandalone() on a real `.lay` file,
    // already translated/height-baked to the building's placement point),
    // chained onto the main planet generator during every subsequent
    // queryHeight()/generateChunk() call - mirrors Core3's real
    // ProceduralTerrainAppearance::addTerrainModification()/
    // removeTerrainModification() (a `customTerrain` list keyed by object
    // id, so a structure's grading can be un-registered when it's
    // destroyed). Thread-safe: takes an exclusive lock, so it may be called
    // from a different thread than the shared-locked query methods (e.g.
    // AssetWorkerThread registering grading while the same or another
    // thread is mid-`generateChunk()`).
    void addTerrainModification(uint64_t objectId, TerrainGenerator generator);
    void removeTerrainModification(uint64_t objectId);

private:
    TrnHeader header_;
    TerrainGenerator generator_;

    // Real Core3 chaining semantics (ProceduralTerrainAppearance::
    // getHeight()): one running Point/height value is threaded through
    // generator_ then every entry here, in insertion order - a
    // TGO_replace affector lerps against whatever the chain already
    // produced, not a generator-local default. See TerrainGenerator::
    // queryPoint()'s chaining overload.
    std::vector<std::pair<uint64_t, TerrainGenerator>> customTerrain_;

    // Guards customTerrain_ - queryHeight()/generateChunk() take a shared
    // (read) lock, add/removeTerrainModification take an exclusive (write)
    // lock. Wrapped in unique_ptr so ProceduralTerrainSource stays
    // move-constructible (a bare std::shared_mutex member isn't movable,
    // and parse() returns this type by value into
    // make_shared<ProceduralTerrainSource>(...) at its real call site).
    mutable std::unique_ptr<std::shared_mutex> customTerrainMutex_ =
        std::make_unique<std::shared_mutex>();
};

} // namespace terrain
