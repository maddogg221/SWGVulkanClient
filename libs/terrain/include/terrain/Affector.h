#pragma once

#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "assets/IffReader.h"
#include "assets/StaticMesh.h"
#include "terrain/LayerItem.h"

namespace terrain {

// Real affector tags, per PHASE_11_STATUS.md and this library's own deep
// research pass (see the terrain Phase 3 commit): AENV, AHTR/AHCN/AHFR,
// ACCN/ACRH/ACRF, ASCN/ASRP, AFCN+AFSC (alias pair, same shape),
// AFSN, ARCN+AFDN (alias pair, same shape), AFDF, ARIB, AEXC, APAS, AROA,
// ARIV. Dead tags (AHSM/AHBM/ACBM/ASBM/AFBM) are still present in some
// real files' streams but produce no live object, matching Boundary's
// BALL/BSPL treatment.
//
// Every implemented type here supports exactly the ONE version real
// classic-planet files were confirmed to use (a real-bytes scan of all
// 2054 layers across all 10 classic .trn files found zero exceptions per
// tag). AffectorRibbon (ARIB) is the one live, non-dead tag that NEVER
// appears in any of the 10 classic files - given its real complexity (6
// historical versions, a HeightData sub-form, end-cap-generation-adjacent
// fields) and zero real evidence of what it should look like, it is
// recognized (isAffectorTag) but deliberately NOT implemented
// (parseAffector throws a clear, specific error) rather than guessing at
// an unverified layout - matching this project's "only implement what's
// confirmed against real data" discipline (see this session's project
// memory on Core3-as-a-map-not-truth, applied here to source-as-a-map).

struct AffectorEnvironment {
    int familyId = 0;
    bool useFeatherClampOverride = false;
    float featherClampOverride = 0.0f;
};

// TerrainGeneratorOperation ordinal isn't independently confirmed from a
// header (inferred from declaration order only) - stored as a raw int
// rather than a possibly-wrong enum.
struct AffectorHeightConstant {
    int operation = 0;
    float height = 0.0f;
};

// familyId references a FractalGroup family (see Family.h).
struct AffectorHeightFractal {
    int familyId = 0;
    int operation = 0;
    float scaleY = 0.0f;
};

struct AffectorHeightTerrace {
    float fraction = 0.0f;
    float height = 0.0f;
};

struct AffectorColorConstant {
    int operation = 0;
    uint8_t colorR = 0;
    uint8_t colorG = 0;
    uint8_t colorB = 0;
};

struct AffectorColorRampHeight {
    int operation = 0;
    float lowHeight = 0.0f;
    float highHeight = 0.0f;
    std::string imageName;
};

// familyId references a FractalGroup family (see Family.h).
struct AffectorColorRampFractal {
    int familyId = 0;
    int operation = 0;
    std::string imageName;
};

// familyId references a ShaderGroup family (see Family.h).
struct AffectorShaderConstant {
    int familyId = 0;
    bool useFeatherClampOverride = false;
    float featherClampOverride = 0.0f;
};

struct AffectorShaderReplace {
    int sourceFamilyId = 0;
    int destinationFamilyId = 0;
    bool useFeatherClampOverride = false;
    float featherClampOverride = 0.0f;
};

// Shared shape for AffectorFloraStaticCollidableConstant (tags AFCN/AFSC)
// and AffectorFloraStaticNonCollidableConstant (tag AFSN) - the real engine
// uses one C++ class for both AFCN/AFSC (they're literal aliases), and an
// identically-shaped sibling class for AFSN.
struct AffectorFloraStatic {
    int familyId = 0;
    int operation = 0;
    bool removeAll = false;
    bool densityOverride = false;
    float densityOverrideDensity = 0.0f;
    bool collidable = true; // true for AFCN/AFSC, false for AFSN - not itself a wire field
};

// Shared shape for AffectorFloraDynamicNearConstant (tags ARCN/AFDN) and
// AffectorFloraDynamicFarConstant (tag AFDF) - same relationship as
// AffectorFloraStatic above.
struct AffectorFloraDynamic {
    int familyId = 0;
    int operation = 0;
    bool removeAll = false;
    bool densityOverride = false;
    float densityOverrideDensity = 0.0f;
    bool near_ = true; // true for ARCN/AFDN, false for AFDF - not a wire field
};

struct AffectorExclude {};

struct AffectorPassable {
    bool passable = false;
    float featherThreshold = 0.0f;
};

// Ported from the real HeightData sub-form (FORM ROAD | HDTA), used by both
// AffectorRoad and AffectorRiver's current versions. Optional in the wire
// format (absent entirely if the affector never had per-point heights
// authored) - each segment is a poly-line of world-space points.
struct HeightDataSegment {
    std::vector<assets::Float3> points;
};
using HeightData = std::vector<HeightDataSegment>;

struct AffectorRoad {
    HeightData heightData;
    std::vector<assets::Float2> points;
    float width = 0.0f;
    int familyId = 0;
    int featherFunction = 0;
    float featherDistance = 0.0f;
    int featherFunctionShader = 0;
    float featherDistanceShader = 0.0f;
};

struct AffectorRiver {
    HeightData heightData;
    std::vector<assets::Float2> points;
    float width = 0.0f;
    int bankFamilyId = 0;
    int bottomFamilyId = 0;
    int featherFunction = 0;
    float featherDistance = 0.0f;
    float trenchDepth = 0.0f;
    float velocity = 0.0f;
    bool hasLocalWaterTable = false;
    float localWaterTableDepth = 0.0f;
    float localWaterTableWidth = 0.0f;
    float localWaterTableShaderSize = 0.0f;
    std::string localWaterTableShaderTemplateName;
};

using AffectorVariant =
    std::variant<AffectorEnvironment, AffectorHeightConstant, AffectorHeightFractal,
                 AffectorHeightTerrace, AffectorColorConstant, AffectorColorRampHeight,
                 AffectorColorRampFractal, AffectorShaderConstant, AffectorShaderReplace,
                 AffectorFloraStatic, AffectorFloraDynamic, AffectorExclude, AffectorPassable,
                 AffectorRoad, AffectorRiver>;

struct Affector {
    LayerItemHeader header;
    AffectorVariant affector;
};

bool isAffectorTag(uint32_t tag);

// `form` is the affector's own FORM node (e.g. `FORM AENV`). Returns
// std::nullopt for the dead tags (AHSM/AHBM/ACBM/ASBM/AFBM). Throws
// std::runtime_error for AffectorRibbon (recognized but not implemented,
// see header comment), an unrecognized tag, or an unsupported version.
std::optional<Affector> parseAffector(const assets::IffChunk& form);

} // namespace terrain
