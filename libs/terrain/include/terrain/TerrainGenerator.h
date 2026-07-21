#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "assets/IffReader.h"
#include "assets/TgaImage.h"
#include "assets/TreArchive.h"
#include "terrain/Family.h"
#include "terrain/Layer.h"
#include "terrain/MultiFractal.h"

namespace terrain {

// The fully-parsed `FORM TGEN` tree: every family group plus the top-level
// Layer list, plus queryHeight()/queryColor()/queryShaderId() - the real
// Boundary/Filter/Affector tree-walk algorithm (recovered from
// TerrainGenerator::Layer::affect() in SWG-Source-ref's git history, same
// commit as everything else in this library) adapted from the real
// engine's chunk-grid generation into a single-point query, since this
// project has no chunk-grid infrastructure to generate into. All three
// query methods share one internal per-point walk (height/color/shader
// affectors can appear in the same layer, gated by the same combined
// boundary/filter amount, so they can't be computed independently without
// redundantly re-deriving each other's state).
struct TerrainGenerator {
    FamilyGroups families;
    std::vector<Layer> topLevelLayers;

    // One terrain::MultiFractal per FractalGroup family, built once here
    // rather than reconstructed (and its Perlin permutation table rebuilt)
    // on every AffectorHeightFractal/FilterFractal/AffectorColorRampFractal
    // evaluation - keyed by FractalFamily::familyId.
    std::unordered_map<int, MultiFractal> fractalNoiseByFamilyId;

    // One assets::TgaImage per distinct color-ramp image referenced by an
    // AffectorColorRampHeight/AffectorColorRampFractal in this generator's
    // layer tree, keyed by the affector's own `imageName` field. Populated
    // by parse() only if a `textureArchive` is supplied - real ramp images
    // are separate files in the same client archive the .trn itself came
    // from (confirmed this session: naboo.trn's own 28 distinct ramp
    // images all live in data_other_00.tre, alongside terrain/naboo.trn
    // itself), not embedded in the .trn file. A missing entry (either
    // because no archive was supplied, or the image genuinely wasn't
    // found) means the affector that needed it silently contributes
    // nothing that query - never a throw, since callers that only care
    // about queryHeight() (e.g. every test through Phase 4) have no reason
    // to supply a texture archive at all.
    std::unordered_map<std::string, assets::TgaImage> rampImageCache;

    // `tgenForm` is a `FORM TGEN` node (e.g. `terrain::TrnFile::generatorForm()`).
    // `textureArchive`, if supplied, is used to resolve and load every
    // real color-ramp image this generator's layer tree references (see
    // rampImageCache above) - omit it for height-only use (e.g.
    // queryHeight()-only callers, most existing tests).
    // Throws std::runtime_error for an unsupported TGEN version or any
    // malformed/unrecognized content within it.
    static TerrainGenerator parse(const assets::IffChunk& tgenForm,
                                   const assets::TreArchive* textureArchive = nullptr);

    // Returns nullptr if familyId doesn't reference any parsed FractalGroup
    // family - callers treat this as "can't happen for a well-formed real
    // file" and let it surface as a query-time throw, not a silent 0.
    const MultiFractal* findFractalNoise(int familyId) const;

    // Returns nullptr if familyId doesn't reference any parsed ShaderGroup
    // family.
    const ShaderFamily* findShaderFamily(int familyId) const;

    // Real-world height at (worldX, worldZ), in meters - the real engine's
    // Boundary/Filter/Affector Layer tree, walked top-level-layers-in-order
    // exactly as TerrainGenerator::Layer::affect() does (boundary amount:
    // max of active boundaries, each through its own feather curve, 1.0 if
    // none active; filter amount: min ("FuzzyAnd") of active filters,
    // short-circuiting at 0; invert flags applied after each combination;
    // a sub-layer's own amount is capped by parentAmount * itsOwnAmount,
    // matching the real "previousAmount" threading exactly).
    //
    // Deliberate, documented departures from the real (chunk-grid)
    // algorithm, since this is a single-point query with no grid to
    // precompute into:
    //  - FilterSlope/FilterDirection need a surface normal, which the real
    //    engine reads from a precomputed chunk-wide normal map. Here a
    //    normal is derived on demand via central-difference height
    //    sampling - and since slope/direction filters could otherwise
    //    recurse into computing a normal for their own neighbor samples,
    //    those neighbor samples are computed with slope/direction filters
    //    forced to "always pass" (bounded, non-recursive by construction).
    //  - FilterBitmap (never observed in any real classic-planet file)
    //    always passes (treated as a neutral 1.0) - it would need loading
    //    an external image as a 2D height/luminance field, a genuinely
    //    different thing from the 1-row color ramps queryColor() already
    //    handles, and there's no real file to verify it against.
    //  - Shader assignment (see queryShaderId()) tracks only the assigned
    //    FractalGroup-style family id, never a specific resolved texture
    //    variant within that family - the real engine additionally does a
    //    position-hashed weighted pick among a family's children
    //    (FastRandomGenerator seeded by CoordinateHash, neither recovered)
    //    to choose a specific texture, but nothing in this project
    //    consumes that yet (texture/material binding remains future work,
    //    same as the mesh-building plan's own "no textures this pass"
    //    scope) and FilterShader/AffectorShaderReplace only ever compare
    //    family ids, never the specific child - so the family-level
    //    assignment this library tracks is already everything real
    //    generation logic needs.
    float queryHeight(float worldX, float worldZ) const;

    // Real-world vertex color at (worldX, worldZ) - defaults to white
    // (255,255,255) if no color affector ever touches this point, matching
    // the real engine's own "zero the chunk's maps to defaults" starting
    // state. AffectorColorRampHeight/Fractal only contribute if this
    // generator was parsed with a texture archive AND the referenced ramp
    // image was actually found (see rampImageCache above) - otherwise they
    // silently no-op, same as a real file whose ramp image is missing from
    // the client install.
    assets::Rgb8 queryColor(float worldX, float worldZ) const;

    // The FractalGroup-style family id of whichever shader was last
    // assigned at (worldX, worldZ) by AffectorShaderConstant/
    // AffectorShaderReplace, or -1 if none ever applied. See the
    // queryHeight() comment above for what's deliberately not tracked
    // (the specific resolved texture variant within that family).
    int queryShaderId(float worldX, float worldZ) const;

    // Height + color + shader together from a single layer-tree walk -
    // queryHeight()/queryColor()/queryShaderId() each redo the whole walk
    // independently, which is wasteful for callers (chunk mesh generation)
    // that need all three at the same point anyway. Confirmed a real,
    // significant cost in practice (a single walk over a real file's full
    // layer tree measured in the hundreds of microseconds - see this
    // session's own perf check) - callers generating many points per chunk
    // should use this, not three separate calls.
    struct Point {
        float height = 0.0f;
        assets::Rgb8 color{255, 255, 255};
        int shaderFamilyId = -1;
    };
    Point queryPoint(float worldX, float worldZ) const;
};

} // namespace terrain
