#pragma once

#include <variant>

#include "assets/IffReader.h"
#include "terrain/LayerItem.h"

namespace terrain {

// Real filter tags: FHGT (height range), FFRA (fractal-noise range), FBIT
// (bitmap luminance range), FSLP (slope/angle range), FDIR (facing-direction
// range), FSHD (current shader match). Unlike Boundary, no filter tag is
// dead - all six are live in the real engine.
//
// FHGT/FFRA/FSLP/FDIR each support exactly the ONE version real classic-
// planet files were confirmed to use (a real-bytes scan of all 10 classic
// .trn files found zero exceptions: FHGT/0002, FFRA/0005, FSLP/0002,
// FDIR/0000). FSHD has only ever had one version. FBIT never appears in any
// of the 10 classic files at all - ported from source (version 0001, the
// current/latest) but NOT real-bytes verified, same caveat TrnFile.cpp
// carries for its own never-observed version 0013.
struct FilterHeight {
    float lowHeight = 0.0f;
    float highHeight = 0.0f;
    int featherFunction = 0;
    float featherDistance = 0.0f;
};

// familyId references a FractalGroup family (see Family.h) - real files
// (FFRA/0005) store this directly; older versions (not supported here)
// embedded a full MultiFractal description instead.
struct FilterFractal {
    int familyId = 0;
    int featherFunction = 0;
    float featherDistance = 0.0f;
    float lowFractalLimit = 0.0f;
    float highFractalLimit = 0.0f;
    float scaleY = 0.0f;
};

// Not real-bytes verified - see header comment.
struct FilterBitmap {
    int familyId = 0;
    int featherFunction = 0;
    float featherDistance = 0.0f;
    float lowBitmapLimit = 0.0f;
    float highBitmapLimit = 0.0f;
    float gain = 0.0f;
};

struct FilterSlope {
    float minimumAngleRadians = 0.0f;
    float maximumAngleRadians = 0.0f;
    int featherFunction = 0;
    float featherDistance = 0.0f;
};

struct FilterDirection {
    float minimumAngleRadians = 0.0f;
    float maximumAngleRadians = 0.0f;
    int featherFunction = 0;
    float featherDistance = 0.0f;
};

// familyId references a ShaderGroup family (see Family.h).
struct FilterShader {
    int familyId = 0;
};

using FilterVariant = std::variant<FilterHeight, FilterFractal, FilterBitmap, FilterSlope,
                                    FilterDirection, FilterShader>;

struct Filter {
    LayerItemHeader header;
    FilterVariant filter;
};

bool isFilterTag(uint32_t tag);

// `form` is the filter's own FORM node (e.g. `FORM FHGT`). Throws
// std::runtime_error for an unrecognized tag or an unsupported version.
Filter parseFilter(const assets::IffChunk& form);

} // namespace terrain
