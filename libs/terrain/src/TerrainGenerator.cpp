#include "terrain/TerrainGenerator.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>
#include <stdexcept>

#include "GeneratorItem.h"
#include "NormalMath.h"
#include "Tags.h"

namespace terrain {

namespace {

void collectRampImageNames(const std::vector<Layer>& layers, std::set<std::string>& names) {
    for (const auto& layer : layers) {
        for (const auto& affector : layer.affectors) {
            std::visit(
                [&](const auto& a) {
                    using T = std::decay_t<decltype(a)>;
                    if constexpr (std::is_same_v<T, AffectorColorRampHeight> ||
                                  std::is_same_v<T, AffectorColorRampFractal>) {
                        names.insert(a.imageName);
                    }
                },
                affector.affector);
        }
        collectRampImageNames(layer.subLayers, names);
    }
}

} // namespace

TerrainGenerator TerrainGenerator::parse(const assets::IffChunk& tgenForm,
                                          const assets::TreArchive* textureArchive) {
    if (tgenForm.formType != kTagTGEN) {
        throw std::runtime_error("terrain: TerrainGenerator::parse called with a non-TGEN form");
    }
    if (tgenForm.children.empty() || tgenForm.children[0].id != assets::kFormTag) {
        throw std::runtime_error("terrain: TGEN missing version FORM");
    }
    const assets::IffChunk& versionForm = tgenForm.children[0];
    if (versionForm.formType != versionTag(0)) {
        throw std::runtime_error("terrain: TGEN unsupported version (only 0000 exists)");
    }

    TerrainGenerator generator;
    generator.families = parseFamilyGroups(versionForm);

    const assets::IffChunk* lyrs = findChildForm(versionForm, kTagLYRS);
    if (lyrs != nullptr) {
        for (const auto& child : lyrs->children) {
            if (child.id != assets::kFormTag || child.formType != kTagLAYR) {
                throw std::runtime_error("terrain: LYRS: unexpected child");
            }
            generator.topLevelLayers.push_back(parseLayer(child));
        }
    } else {
        // A real standalone `.lay` terrain-modification file has no FORM
        // LYRS wrapper at all - its top-level layer(s) sit directly as
        // FORM LAYR siblings of SGRP/FGRP/RGRP/EGRP/MGRP (confirmed via a
        // real-bytes walk of terrain/ply_tatt_house_sml_s01.lay; see
        // Family.cpp's own kTagLAYR case, which tolerates rather than
        // throws on this shape). A real .trn always has LYRS, so this
        // fallback only ever fires for the standalone-.lay path.
        for (const auto& child : versionForm.children) {
            if (child.id == assets::kFormTag && child.formType == kTagLAYR) {
                generator.topLevelLayers.push_back(parseLayer(child));
            }
        }
    }

    for (const FractalFamily& family : generator.families.fractalFamilies) {
        generator.fractalNoiseByFamilyId.emplace(family.familyId, MultiFractal(family.params));
    }

    if (textureArchive != nullptr) {
        std::set<std::string> rampImageNames;
        collectRampImageNames(generator.topLevelLayers, rampImageNames);
        for (const auto& name : rampImageNames) {
            if (!textureArchive->contains(name)) {
                continue;
            }
            try {
                generator.rampImageCache.emplace(name,
                                                  assets::TgaImage::parse(textureArchive->extract(name)));
            } catch (const std::exception&) {
                // A real but malformed/unsupported ramp image - the
                // affector that needed it silently no-ops, same as if the
                // archive didn't have it at all. Not a parse-time failure.
            }
        }
    }

    return generator;
}

TerrainGenerator TerrainGenerator::parseStandalone(const std::vector<uint8_t>& layBytes,
                                                     const assets::TreArchive* textureArchive) {
    // A real standalone `.lay` terrain-modification file's raw IffReader
    // parse is a flat list of top-level sibling FORMs (SGRP/FGRP/RGRP/EGRP/
    // MGRP/LAYR) with no enclosing FORM TGEN -> FORM 0000 wrapper, unlike a
    // real .trn (confirmed via a real-bytes walk of
    // terrain/ply_tatt_house_sml_s01.lay). Synthesize that wrapper in memory
    // and delegate to parse() unchanged, rather than duplicating its logic.
    std::vector<assets::IffChunk> topLevel = assets::IffReader::parse(layBytes);

    assets::IffChunk versionForm;
    versionForm.id = assets::kFormTag;
    versionForm.formType = versionTag(0);
    versionForm.children = std::move(topLevel);

    assets::IffChunk tgenForm;
    tgenForm.id = assets::kFormTag;
    tgenForm.formType = kTagTGEN;
    tgenForm.children.push_back(std::move(versionForm));

    return parse(tgenForm, textureArchive);
}

const MultiFractal* TerrainGenerator::findFractalNoise(int familyId) const {
    auto it = fractalNoiseByFamilyId.find(familyId);
    return it == fractalNoiseByFamilyId.end() ? nullptr : &it->second;
}

const ShaderFamily* TerrainGenerator::findShaderFamily(int familyId) const {
    for (const auto& family : families.shaderFamilies) {
        if (family.familyId == familyId) {
            return &family;
        }
    }
    return nullptr;
}

namespace {

inline float sqr(float x) {
    return x * x;
}

inline float lerp(float a, float b, float t) {
    return a + t * (b - a);
}

constexpr float kPiOver2 = std::numbers::pi_v<float> / 2.0f;

// Per-point generation state, threaded through one layer walk - height,
// color, and shader affectors can all appear in the same layer, gated by
// the same combined boundary/filter amount, so they're computed together
// rather than via three independent walks (which would also need to
// redundantly re-derive each other's state for filters like FilterHeight/
// FilterShader that depend on it). Reuses the public TerrainGenerator::Point
// shape directly rather than a separate identical struct, so queryPoint()
// doesn't need to field-copy from one into the other.
using GeneratorPoint = TerrainGenerator::Point;

// Ported from TerrainGenerator.cpp's file-local FuzzyAnd/FuzzyOr (fuzzy
// logic AND/OR = min/max) and Feather::feather() - ordinals confirmed from
// the real TerrainGeneratorType.def (TGFF_linear=0, easeIn=1, easeOut=2,
// easeInOut=3).
float applyFeatherCurve(int featherFunction, float t) {
    switch (featherFunction) {
        case 0: // TGFF_linear
            return t;
        case 1: // TGFF_easeIn
            return t * t;
        case 2: // TGFF_easeOut
            return std::sqrt(std::max(t, 0.0f));
        case 3: // TGFF_easeInOut
            return (3.0f - 2.0f * t) * t * t;
        default:
            return t;
    }
}

// Ported from Filter.cpp's file-local computeFeatheredInterpolant() -
// shared by FilterHeight/FilterFractal/FilterSlope/FilterBitmap in the
// real engine.
float computeFeatheredInterpolant(float minimum, float value, float maximum, float featherIn) {
    if (!(value > minimum && value < maximum)) {
        return 0.0f;
    }
    float feather = featherIn * (maximum - minimum) * 0.5f;
    if (value < minimum + feather) {
        return (value - minimum) / feather;
    }
    if (value > maximum - feather) {
        return (maximum - value) / feather;
    }
    return 1.0f;
}

// -- Boundary::isWithin, ported exactly from Boundary.cpp --

float circleIsWithin(const BoundaryCircle& c, float worldX, float worldZ) {
    float distSq = sqr(c.centerX - worldX) + sqr(c.centerZ - worldZ);
    float radiusSq = sqr(c.radius);
    if (distSq > radiusSq) {
        return 0.0f;
    }
    float innerRadiusSq = sqr(c.radius * (1.0f - c.featherDistance));
    if (distSq <= innerRadiusSq) {
        return 1.0f;
    }
    return 1.0f - (distSq - innerRadiusSq) / (radiusSq - innerRadiusSq);
}

float rectangleIsWithin(const BoundaryRectangle& r, float worldX, float worldZ) {
    if (worldX < r.x0 || worldX > r.x1 || worldZ < r.y0 || worldZ > r.y1) {
        return 0.0f;
    }
    if (r.featherDistance == 0.0f) {
        return 1.0f;
    }
    float width = r.x1 - r.x0;
    float height = r.y1 - r.y0;
    float feather = 0.5f * std::min(width, height) * r.featherDistance;
    float distance = feather;
    distance = std::min(distance, worldX - r.x0);
    distance = std::min(distance, r.x1 - worldX);
    distance = std::min(distance, worldZ - r.y0);
    distance = std::min(distance, r.y1 - worldZ);
    return distance / feather;
}

bool pointInPolygon(const std::vector<assets::Float2>& pts, float worldX, float worldZ) {
    bool in = false;
    int n = static_cast<int>(pts.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        if (((pts[i].y <= worldZ && worldZ < pts[j].y) ||
             (pts[j].y <= worldZ && worldZ < pts[i].y)) &&
            (worldX < (pts[j].x - pts[i].x) * (worldZ - pts[i].y) / (pts[j].y - pts[i].y) +
                          pts[i].x)) {
            in = !in;
        }
    }
    return in;
}

// Note: unlike Circle/Rectangle/Polyline (where featherDistance is a
// [0,1] fraction of the shape's own size), Polygon's featherDistance is
// used directly as an absolute world-unit (meter) distance - a confirmed
// real-engine quirk (Boundary.cpp's BoundaryPolygon::isWithin), not a
// transcription mistake.
float polygonIsWithin(const BoundaryPolygon& poly, float worldX, float worldZ) {
    if (!pointInPolygon(poly.points, worldX, worldZ)) {
        return 0.0f;
    }
    if (poly.featherDistance == 0.0f) {
        return 1.0f;
    }

    float featherDistSq = sqr(poly.featherDistance);
    float distSq = featherDistSq;
    int n = static_cast<int>(poly.points.size());
    for (int i = 0; i < n; ++i) {
        distSq = std::min(distSq, sqr(worldX - poly.points[i].x) + sqr(worldZ - poly.points[i].y));
    }
    for (int i = 0, j = n - 1; i < n; j = i++) {
        float x1 = poly.points[j].x, y1 = poly.points[j].y;
        float x2 = poly.points[i].x, y2 = poly.points[i].y;
        float denom = sqr(x1 - x2) + sqr(y1 - y2);
        if (denom > 0.0f) {
            float u = ((worldX - x1) * (x2 - x1) + (worldZ - y1) * (y2 - y1)) / denom;
            if (u >= 0.0f && u <= 1.0f) {
                float px = x1 + u * (x2 - x1);
                float pz = y1 + u * (y2 - y1);
                distSq = std::min(distSq, sqr(worldX - px) + sqr(worldZ - pz));
            }
        }
    }

    if (std::fabs(featherDistSq - distSq) > 0.0001f) {
        return std::sqrt(distSq) / poly.featherDistance;
    }
    return 1.0f;
}

float polylineIsWithin(const BoundaryPolyline& line, float worldX, float worldZ) {
    float widthSq = sqr(line.width);
    float distSq = widthSq;
    for (const auto& p : line.points) {
        distSq = std::min(distSq, sqr(worldX - p.x) + sqr(worldZ - p.y));
    }
    int n = static_cast<int>(line.points.size()) - 1;
    for (int i = 0; i < n; ++i) {
        float x1 = line.points[i].x, y1 = line.points[i].y;
        float x2 = line.points[i + 1].x, y2 = line.points[i + 1].y;
        float denom = sqr(x2 - x1) + sqr(y2 - y1);
        if (denom > 0.0f) {
            float u = ((worldX - x1) * (x2 - x1) + (worldZ - y1) * (y2 - y1)) / denom;
            if (u >= 0.0f && u <= 1.0f) {
                float px = x1 + u * (x2 - x1);
                float pz = y1 + u * (y2 - y1);
                distSq = std::min(distSq, sqr(worldX - px) + sqr(worldZ - pz));
            }
        }
    }

    if (distSq < widthSq) {
        float newFeather = line.width * (1.0f - line.featherDistance);
        float newFeatherSq = sqr(newFeather);
        if (distSq < newFeatherSq) {
            return 1.0f;
        }
        return 1.0f - (std::sqrt(distSq) - newFeather) / (line.width - newFeather);
    }
    return 0.0f;
}

// Returns this boundary's own feathered [0,1] amount (both the shape's own
// internal feathering AND the outer per-boundary feather-curve remap
// already applied) - ready to be max-combined with its siblings. Inactive
// boundaries contribute 0 (matching the real engine only scan-converting
// active ones).
float boundaryAmount(const Boundary& b, float worldX, float worldZ) {
    if (!b.header.active) {
        return 0.0f;
    }
    float raw = 0.0f;
    int featherFunction = 0;
    std::visit(
        [&](const auto& shape) {
            using T = std::decay_t<decltype(shape)>;
            featherFunction = shape.featherFunction;
            if constexpr (std::is_same_v<T, BoundaryCircle>) {
                raw = circleIsWithin(shape, worldX, worldZ);
            } else if constexpr (std::is_same_v<T, BoundaryRectangle>) {
                raw = rectangleIsWithin(shape, worldX, worldZ);
            } else if constexpr (std::is_same_v<T, BoundaryPolygon>) {
                raw = polygonIsWithin(shape, worldX, worldZ);
            } else {
                raw = polylineIsWithin(shape, worldX, worldZ);
            }
        },
        b.shape);
    return applyFeatherCurve(featherFunction, raw);
}

// Ported from TerrainGenerator::Layer::affect()'s boundary-combination
// pass: max ("FuzzyOr") of active boundaries, 1.0 if none active, then
// invertBoundaries applied unconditionally afterward (including the "no
// active boundaries" default - a layer with invertBoundaries=true and no
// boundaries at all really does compute 0.0 everywhere in the real
// engine; not "fixed" here).
float computeBoundaryCombined(const Layer& layer, float worldX, float worldZ) {
    bool anyActive = false;
    for (const auto& b : layer.boundaries) {
        if (b.header.active) {
            anyActive = true;
            break;
        }
    }
    float fuzzyTest = 1.0f;
    if (anyActive) {
        fuzzyTest = 0.0f;
        for (const auto& b : layer.boundaries) {
            fuzzyTest = std::max(fuzzyTest, boundaryAmount(b, worldX, worldZ));
        }
    }
    if (layer.invertBoundaries) {
        fuzzyTest = 1.0f - fuzzyTest;
    }
    return fuzzyTest;
}

// A surface normal derived on demand via central-difference height
// sampling - an original design (no real client mesh/normal-generation
// code survives, see PHASE_11_STATUS.md), applied here at generation time
// (not just render time) since FilterSlope/FilterDirection need one.
// `queryPointImpl`'s `allowSlopeDirection=false` on these neighbor samples
// is what keeps this bounded rather than recursive.
constexpr float kNormalSampleDistanceMeters = 1.0f;

GeneratorPoint queryPointImpl(const TerrainGenerator& gen, float worldX, float worldZ,
                               bool allowSlopeDirection, GeneratorPoint point);

assets::Float3 computeNormal(const TerrainGenerator& gen, float worldX, float worldZ) {
    float hL = queryPointImpl(gen, worldX - kNormalSampleDistanceMeters, worldZ, false, GeneratorPoint{})
                   .height;
    float hR = queryPointImpl(gen, worldX + kNormalSampleDistanceMeters, worldZ, false, GeneratorPoint{})
                   .height;
    float hD = queryPointImpl(gen, worldX, worldZ - kNormalSampleDistanceMeters, false, GeneratorPoint{})
                   .height;
    float hU = queryPointImpl(gen, worldX, worldZ + kNormalSampleDistanceMeters, false, GeneratorPoint{})
                   .height;
    return normalFromHeightSamples(hL, hR, hD, hU, kNormalSampleDistanceMeters);
}

// Returns this filter's final [0,1] amount, ready for FuzzyAnd (min)
// combination with its siblings - both the filter's own internal
// feathering (computeFeatheredInterpolant, using its featherDistance) AND
// the outer per-filter feather-curve remap already applied, matching the
// real engine's `FuzzyAnd(fuzzyTest, feather.feather(0,1,amount))`.
float filterAmount(const Filter& f, float worldX, float worldZ, const GeneratorPoint& point,
                    const TerrainGenerator& gen, bool allowSlopeDirection) {
    float raw = 1.0f;
    int featherFunction = 0;
    std::visit(
        [&](const auto& filter) {
            using T = std::decay_t<decltype(filter)>;
            if constexpr (std::is_same_v<T, FilterHeight>) {
                featherFunction = filter.featherFunction;
                raw = computeFeatheredInterpolant(filter.lowHeight, point.height,
                                                   filter.highHeight, filter.featherDistance);
            } else if constexpr (std::is_same_v<T, FilterFractal>) {
                featherFunction = filter.featherFunction;
                const MultiFractal* noise = gen.findFractalNoise(filter.familyId);
                if (noise == nullptr) {
                    throw std::runtime_error(
                        "terrain: FilterFractal references an unknown fractal family");
                }
                float fractalHeight = filter.scaleY * noise->noise2D(worldX, worldZ);
                raw = computeFeatheredInterpolant(filter.lowFractalLimit, fractalHeight,
                                                   filter.highFractalLimit,
                                                   filter.featherDistance);
            } else if constexpr (std::is_same_v<T, FilterSlope>) {
                featherFunction = filter.featherFunction;
                if (!allowSlopeDirection) {
                    raw = 1.0f;
                } else {
                    assets::Float3 normal = computeNormal(gen, worldX, worldZ);
                    float clampedMin = std::clamp(filter.minimumAngleRadians, 0.0f, kPiOver2);
                    float clampedMax = std::clamp(filter.maximumAngleRadians, 0.0f, kPiOver2);
                    float sinMinAngle = std::sin(kPiOver2 - clampedMin);
                    float sinMaxAngle = std::sin(kPiOver2 - clampedMax);
                    raw = computeFeatheredInterpolant(sinMaxAngle, normal.y, sinMinAngle,
                                                       filter.featherDistance);
                }
            } else if constexpr (std::is_same_v<T, FilterDirection>) {
                featherFunction = 0;
                if (!allowSlopeDirection) {
                    raw = 1.0f;
                } else {
                    // Real files: 2 occurrences out of 2054 real layers -
                    // implemented best-effort (a standard atan2-based
                    // horizontal bearing) rather than researched to the
                    // same real-bytes-verified bar as everything else in
                    // this library; not expected to be exercised by any
                    // currently-used real content.
                    assets::Float3 normal = computeNormal(gen, worldX, worldZ);
                    float theta = std::atan2(normal.x, normal.z);
                    float normalizedTheta =
                        theta / (2.0f * std::numbers::pi_v<float>) + 0.5f;
                    float clampedMin = std::clamp(filter.minimumAngleRadians, 0.0f, kPiOver2);
                    float clampedMax = std::clamp(filter.maximumAngleRadians, 0.0f, kPiOver2);
                    raw = computeFeatheredInterpolant(clampedMin, normalizedTheta, clampedMax,
                                                       filter.featherDistance);
                }
            } else if constexpr (std::is_same_v<T, FilterShader>) {
                raw = (point.shaderFamilyId == filter.familyId) ? 1.0f : 0.0f;
            } else { // FilterBitmap
                // Never observed in any real classic-planet file; would
                // also need external .tga loading as a 2D field (not the
                // 1-row ramps queryColor() handles), out of scope here.
                raw = 1.0f;
            }
        },
        f.filter);
    return applyFeatherCurve(featherFunction, raw);
}

// TerrainGeneratorOperation ordinals, confirmed from the real
// TerrainGeneratorType.def: TGO_replace=0, TGO_add=1, TGO_subtract=2,
// TGO_multiply=3.
float applyOperation(int operation, float oldValue, float newValue, float amount) {
    switch (operation) {
        case 1: // TGO_add
            return oldValue + amount * newValue;
        case 2: // TGO_subtract
            return oldValue - amount * newValue;
        case 3: { // TGO_multiply
            float desired = oldValue * newValue;
            return lerp(oldValue, desired, amount);
        }
        case 0: // TGO_replace
        default:
            return lerp(oldValue, newValue, amount);
    }
}

// Ported exactly from AffectorColor.cpp's file-local computeColor().
assets::Rgb8 applyColorOperation(int operation, const assets::Rgb8& oldColor,
                                  const assets::Rgb8& desiredColor, float amount) {
    assets::Rgb8 newColor = desiredColor;
    if (amount < 1.0f) {
        newColor.r = static_cast<uint8_t>(desiredColor.r * amount);
        newColor.g = static_cast<uint8_t>(desiredColor.g * amount);
        newColor.b = static_cast<uint8_t>(desiredColor.b * amount);
    }
    switch (operation) {
        case 1: // TGO_add
            newColor.r = static_cast<uint8_t>(std::min(oldColor.r + newColor.r, 255));
            newColor.g = static_cast<uint8_t>(std::min(oldColor.g + newColor.g, 255));
            newColor.b = static_cast<uint8_t>(std::min(oldColor.b + newColor.b, 255));
            break;
        case 2: // TGO_subtract
            newColor.r = static_cast<uint8_t>(std::max(oldColor.r - newColor.r, 0));
            newColor.g = static_cast<uint8_t>(std::max(oldColor.g - newColor.g, 0));
            newColor.b = static_cast<uint8_t>(std::max(oldColor.b - newColor.b, 0));
            break;
        case 3: // TGO_multiply
            newColor.r = static_cast<uint8_t>(amount * (0.5f * desiredColor.r + 0.5f * oldColor.r) +
                                               (1.0f - amount) * oldColor.r);
            newColor.g = static_cast<uint8_t>(amount * (0.5f * desiredColor.g + 0.5f * oldColor.g) +
                                               (1.0f - amount) * oldColor.g);
            newColor.b = static_cast<uint8_t>(amount * (0.5f * desiredColor.b + 0.5f * oldColor.b) +
                                               (1.0f - amount) * oldColor.b);
            break;
        case 0: // TGO_replace
        default:
            newColor.r =
                static_cast<uint8_t>(amount * desiredColor.r + (1.0f - amount) * oldColor.r);
            newColor.g =
                static_cast<uint8_t>(amount * desiredColor.g + (1.0f - amount) * oldColor.g);
            newColor.b =
                static_cast<uint8_t>(amount * desiredColor.b + (1.0f - amount) * oldColor.b);
            break;
    }
    return newColor;
}

// Ported from AffectorColor.cpp's file-local getPixel() (nearest-index
// sample, no interpolation between adjacent ramp pixels).
assets::Rgb8 sampleRamp(const assets::TgaImage& image, float t) {
    if (image.width <= 0) {
        return assets::Rgb8{255, 255, 255}; // matches the "no ramp data" default elsewhere
    }
    int idx = static_cast<int>(t * static_cast<float>(image.width - 1));
    idx = std::clamp(idx, 0, image.width - 1);
    return image.pixels[static_cast<size_t>(idx)];
}

// Applies one affector's effect (whichever domain it belongs to) into
// `point`, blended by `amount` exactly as the real engine's own affect()
// methods do. Affector types outside height/color/shader (flora, road,
// river, exclude, passable, environment, ribbon) are no-ops here, matching
// their real getAffectedMaps() never including TGM_height/TGM_color/
// TGM_shader - genuinely out of scope for these three query methods.
void applyAffector(const Affector& affector, float worldX, float worldZ, float amount,
                    const TerrainGenerator& gen, GeneratorPoint& point) {
    if (amount <= 0.0f || !affector.header.active) {
        return;
    }
    std::visit(
        [&](const auto& a) {
            using T = std::decay_t<decltype(a)>;
            if constexpr (std::is_same_v<T, AffectorHeightConstant>) {
                point.height = applyOperation(a.operation, point.height, a.height, amount);
            } else if constexpr (std::is_same_v<T, AffectorHeightFractal>) {
                const MultiFractal* noise = gen.findFractalNoise(a.familyId);
                if (noise == nullptr) {
                    throw std::runtime_error(
                        "terrain: AffectorHeightFractal references an unknown fractal family");
                }
                float fractalHeight = a.scaleY * noise->noise2D(worldX, worldZ);
                point.height = applyOperation(a.operation, point.height, fractalHeight, amount);
            } else if constexpr (std::is_same_v<T, AffectorHeightTerrace>) {
                if (a.height > 0.0f) {
                    float originalHeight = point.height;
                    float terraceHeight = a.height;
                    float lowHeight =
                        originalHeight - ((originalHeight < 0.0f)
                                               ? (terraceHeight +
                                                  std::fmod(originalHeight, terraceHeight))
                                               : std::fmod(originalHeight, terraceHeight));
                    float midHeight = lowHeight + terraceHeight * a.fraction;
                    float highHeight = lowHeight + terraceHeight;

                    float newHeight = lowHeight;
                    if (originalHeight > midHeight) {
                        float t = (originalHeight - midHeight) / (highHeight - midHeight);
                        newHeight = lerp(lowHeight, highHeight, t);
                    }
                    point.height = lerp(originalHeight, newHeight, amount);
                }
            } else if constexpr (std::is_same_v<T, AffectorColorConstant>) {
                assets::Rgb8 desired{a.colorR, a.colorG, a.colorB};
                point.color = applyColorOperation(a.operation, point.color, desired, amount);
            } else if constexpr (std::is_same_v<T, AffectorColorRampHeight>) {
                auto it = gen.rampImageCache.find(a.imageName);
                if (it != gen.rampImageCache.end() && point.height >= a.lowHeight &&
                    point.height <= a.highHeight) {
                    float t = (point.height - a.lowHeight) / (a.highHeight - a.lowHeight);
                    assets::Rgb8 desired = sampleRamp(it->second, t);
                    point.color = applyColorOperation(a.operation, point.color, desired, amount);
                }
            } else if constexpr (std::is_same_v<T, AffectorColorRampFractal>) {
                auto it = gen.rampImageCache.find(a.imageName);
                const MultiFractal* noise = gen.findFractalNoise(a.familyId);
                if (it != gen.rampImageCache.end() && noise != nullptr) {
                    float t = noise->noise2D(worldX, worldZ);
                    assets::Rgb8 desired = sampleRamp(it->second, t);
                    point.color = applyColorOperation(a.operation, point.color, desired, amount);
                }
            } else if constexpr (std::is_same_v<T, AffectorShaderConstant>) {
                const ShaderFamily* family = gen.findShaderFamily(a.familyId);
                float featherClamp =
                    a.useFeatherClampOverride ? a.featherClampOverride
                                               : (family != nullptr ? family->featherClamp : 0.0f);
                if (amount >= featherClamp) {
                    point.shaderFamilyId = a.familyId;
                }
            } else if constexpr (std::is_same_v<T, AffectorShaderReplace>) {
                if (point.shaderFamilyId == a.sourceFamilyId) {
                    const ShaderFamily* family = gen.findShaderFamily(a.destinationFamilyId);
                    float featherClamp = a.useFeatherClampOverride
                                              ? a.featherClampOverride
                                              : (family != nullptr ? family->featherClamp : 0.0f);
                    if (amount >= featherClamp) {
                        point.shaderFamilyId = a.destinationFamilyId;
                    }
                }
            }
            // else: not a height/color/shader-domain affector, no-op.
        },
        affector.affector);
}

// Ported from TerrainGenerator::Layer::affect() - see TerrainGenerator.h's
// own comments for the documented point-query adaptations (normals,
// FilterBitmap, and shader child-variant resolution).
void applyLayer(const Layer& layer, float worldX, float worldZ, float previousAmount,
                 const TerrainGenerator& gen, bool allowSlopeDirection, GeneratorPoint& point) {
    if (!layer.header.active) {
        return;
    }

    float fuzzyTest = computeBoundaryCombined(layer, worldX, worldZ);

    if (fuzzyTest > 0.0f) {
        for (const auto& filter : layer.filters) {
            if (!filter.header.active) {
                continue;
            }
            float amt = filterAmount(filter, worldX, worldZ, point, gen, allowSlopeDirection);
            fuzzyTest = std::min(fuzzyTest, amt);
            if (fuzzyTest <= 0.0f) {
                break;
            }
        }

        if (layer.invertFilters) {
            fuzzyTest = 1.0f - fuzzyTest;
        }

        if (fuzzyTest > 0.0f) {
            float affectorAmount = fuzzyTest * previousAmount;
            for (const auto& affector : layer.affectors) {
                applyAffector(affector, worldX, worldZ, affectorAmount, gen, point);
            }
        }
    }

    float combinedAmount = fuzzyTest * previousAmount;
    for (const auto& subLayer : layer.subLayers) {
        applyLayer(subLayer, worldX, worldZ, combinedAmount, gen, allowSlopeDirection, point);
    }
}

GeneratorPoint queryPointImpl(const TerrainGenerator& gen, float worldX, float worldZ,
                               bool allowSlopeDirection, GeneratorPoint point) {
    for (const auto& layer : gen.topLevelLayers) {
        applyLayer(layer, worldX, worldZ, 1.0f, gen, allowSlopeDirection, point);
    }
    return point;
}

} // namespace

float TerrainGenerator::queryHeight(float worldX, float worldZ) const {
    return queryPointImpl(*this, worldX, worldZ, true, GeneratorPoint{}).height;
}

assets::Rgb8 TerrainGenerator::queryColor(float worldX, float worldZ) const {
    return queryPointImpl(*this, worldX, worldZ, true, GeneratorPoint{}).color;
}

int TerrainGenerator::queryShaderId(float worldX, float worldZ) const {
    return queryPointImpl(*this, worldX, worldZ, true, GeneratorPoint{}).shaderFamilyId;
}

TerrainGenerator::Point TerrainGenerator::queryPoint(float worldX, float worldZ) const {
    return queryPointImpl(*this, worldX, worldZ, true, GeneratorPoint{});
}

TerrainGenerator::Point TerrainGenerator::queryPoint(float worldX, float worldZ,
                                                      Point startingPoint) const {
    // Real Core3 chaining semantics (ProceduralTerrainAppearance::getHeight()):
    // one running Point is threaded sequentially through the main planet
    // generator, then every registered per-building terrain-modification
    // generator in insertion order - a TGO_replace affector lerps against
    // whatever the chain already produced, not a generator-local default.
    // This overload lets a caller (ProceduralTerrainSource, chaining
    // multiple TerrainGenerator instances) supply that running value instead
    // of always restarting from Point{}.
    return queryPointImpl(*this, worldX, worldZ, true, startingPoint);
}

} // namespace terrain
