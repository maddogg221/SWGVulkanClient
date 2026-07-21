#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/IffReader.h"
#include "terrain/MultiFractal.h"

namespace terrain {

// The 6 "family group" types (`ShaderGroup`/`FloraGroup`/`RadialGroup`/
// `EnvironmentGroup`/`FractalGroup`/`BitmapGroup`) that live directly under
// `FORM TGEN`'s version-0000 form, each optional. Deliberately concrete
// per-group structs rather than one generic weighted-pick template - a
// close read of the real source (this library's Phase 3 research) showed
// Shader/Flora/Radial really do share a "family of named, weighted
// children" shape, but Environment/Fractal/Bitmap don't have a weighted-
// children concept at all (each family IS its one payload) - forcing them
// into the same template would misrepresent the real wire format.
//
// Every group here supports exactly the ONE version real classic-planet
// files were confirmed to use (a real-bytes scan of all 10 classic .trn
// files found zero exceptions per group: SGRP/0006, FGRP/0008, RGRP/0003 -
// notably NOT RadialGroup's newest source version 0004, EGRP/0002 [0000-
// 0002 are byte-identical anyway], MGRP/0000 [the only version that
// exists]).

struct ShaderFamilyChild {
    std::string shaderTemplateName;
    float weight = 0.0f;
};

struct ShaderFamily {
    int familyId = 0;
    std::string name;
    std::string surfacePropertiesName;
    uint8_t colorR = 0, colorG = 0, colorB = 0;
    float shaderSize = 0.0f;
    float featherClamp = 0.0f;
    std::vector<ShaderFamilyChild> children;
};

struct FloraFamilyChild {
    std::string appearanceTemplateName;
    float weight = 0.0f;
    bool shouldSway = false;
    float displacement = 0.0f;
    float period = 0.0f;
    bool alignToTerrain = false;
    bool shouldScale = false;
    float minimumScale = 0.0f;
    float maximumScale = 0.0f;
};

struct FloraFamily {
    int familyId = 0;
    std::string name;
    uint8_t colorR = 0, colorG = 0, colorB = 0;
    float density = 0.0f;
    bool floats = false;
    std::vector<FloraFamilyChild> children;
};

struct RadialFamilyChild {
    std::string shaderTemplateName;
    float weight = 0.0f;
    float distance = 0.0f;
    // Real files (RGRP/0003) only ever have one width/height value, not
    // independent min/max (that's source version 0004, never observed) -
    // width/height apply as both min and max.
    float width = 0.0f;
    float height = 0.0f;
    bool shouldSway = false;
    float displacement = 0.0f;
    float period = 0.0f;
    bool alignToTerrain = false;
    bool createPlus = false;
};

struct RadialFamily {
    int familyId = 0;
    std::string name;
    uint8_t colorR = 0, colorG = 0, colorB = 0;
    float density = 0.0f;
    std::vector<RadialFamilyChild> children;
};

// No weighted children - each family directly represents one environment/
// weather template selection.
struct EnvironmentFamily {
    int familyId = 0;
    std::string name;
    uint8_t colorR = 0, colorG = 0, colorB = 0;
    float featherClamp = 0.0f;
};

// No weighted children, no "children" plural at all - each family owns
// exactly one embedded MultiFractal (FORM MFRC).
struct FractalFamily {
    int familyId = 0;
    std::string name;
    MultiFractalParams params;
};

// No weighted children - each family references exactly one external
// bitmap. `bitmapName` is a path to a `.tga`-family image, resolved
// out-of-band (not part of the `.trn` byte stream) - kept only as a
// string, never loaded here.
struct BitmapFamily {
    int familyId = 0;
    std::string name;
    std::string bitmapName;
};

struct FamilyGroups {
    std::vector<ShaderFamily> shaderFamilies;
    std::vector<FloraFamily> floraFamilies;
    std::vector<RadialFamily> radialFamilies;
    std::vector<EnvironmentFamily> environmentFamilies;
    std::vector<FractalFamily> fractalFamilies;
    std::vector<BitmapFamily> bitmapFamilies;
};

// `tgenVersionForm` is `TGEN`'s own version-0000 form (i.e. `FORM TGEN`'s
// first child). Walks SGRP/FGRP/RGRP/EGRP/MGRP(fractal)/MGRP(bitmap) in
// that fixed order - every one is individually optional, and the two MGRP
// forms (fractal, then bitmap) share identical tags, distinguished only by
// read order (real files: bitmap MGRP is absent in all 10 classic
// planets). Does not consume/return LYRS - that's Layer.h's concern,
// parsed separately by the caller (see TerrainGenerator::parse).
FamilyGroups parseFamilyGroups(const assets::IffChunk& tgenVersionForm);

} // namespace terrain
