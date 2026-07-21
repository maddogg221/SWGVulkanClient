#pragma once

#include <cstdint>
#include <string>

#include "assets/IffReader.h"

namespace terrain {

// A .trn file's top-level container and header fields - the FORM PTAT (or
// legacy alias MPTA) -> version FORM (0013/0014/0015) -> CHNK DATA layout,
// ported from the real original client engine's
// ProceduralTerrainAppearanceTemplate::load()/_load() (sharedTerrain, see
// PHASE_11_STATUS.md section 2 for the full research trace this was built
// from), verified byte-for-byte against a real terrain/naboo.trn extracted
// from this project's own client install via assets::TreArchive.
//
// Deliberately does NOT walk into FORM TGEN (the actual generator) or
// BakedTerrain - this type just locates them and hands back the raw
// assets::IffChunk node, matching this pass' "container parse only, no
// interpretation yet" scope (PHASE_11_STATUS.md's plan, phase 1).
struct TrnHeader {
    std::string name;
    float mapWidthInMeters = 0.0f;
    float chunkWidthInMeters = 0.0f;
    int32_t numberOfTilesPerChunk = 0;
    bool useGlobalWaterTable = false;
    float globalWaterTableHeight = 0.0f;
    float globalWaterTableShaderSize = 0.0f;
    std::string globalWaterTableShaderTemplateName;
    float environmentCycleTime = 0.0f;

    struct FloraPlacementParams {
        float minimumDistance = 0.0f;
        float maximumDistance = 0.0f;
        float tileSize = 0.0f;
        float tileBorder = 0.0f;
        uint32_t seed = 0;
    };
    FloraPlacementParams collidable;
    FloraPlacementParams nonCollidable;
    FloraPlacementParams radial;
    FloraPlacementParams farRadial;

    // Only present (and meaningful) for version >= 0015 - false for older
    // real files, not just a default.
    bool legacyMap = false;

    // tileWidthInMeters isn't stored on the wire - the real engine computes
    // it (ProceduralTerrainAppearanceTemplate, confirmed) as
    // chunkWidthInMeters / numberOfTilesPerChunk every time it's needed,
    // matching that exactly rather than caching a value that could drift
    // from the two fields it's derived from.
    float tileWidthInMeters() const {
        return numberOfTilesPerChunk != 0 ? chunkWidthInMeters / numberOfTilesPerChunk : 0.0f;
    }
};

class TrnFile {
public:
    // Throws std::runtime_error for a missing/unrecognized top-level FORM
    // or version, matching every other parser in this project (e.g.
    // assets::StaticMesh::parse) - malformed input throws, never a silent
    // best-effort parse.
    static TrnFile parse(const std::vector<uint8_t>& bytes);

    const TrnHeader& header() const { return header_; }

    // The raw, unwalked FORM TGEN node - phase 3's job to interpret.
    const assets::IffChunk& generatorForm() const { return *generatorForm_; }

private:
    TrnHeader header_;
    const assets::IffChunk* generatorForm_ = nullptr;
    // Keeps the parsed chunk tree alive for the lifetime of this TrnFile,
    // since generatorForm_ points into it - avoids copying the (large,
    // deeply nested) TGEN subtree just to hand back a view of it.
    std::vector<assets::IffChunk> topLevel_;
};

} // namespace terrain
