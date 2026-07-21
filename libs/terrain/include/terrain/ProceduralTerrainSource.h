#pragma once

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

private:
    TrnHeader header_;
    TerrainGenerator generator_;
};

} // namespace terrain
