// Verification for real terrain grading around player structures (Feature 1
// of the terrain-grading/portal-visibility plan): TerrainGenerator::
// parseStandalone() on a real standalone `.lay` file, translateLayerBoundaries/
// bakeLayerHeight, and TerrainGenerator::queryPoint()'s chaining overload
// (the primitive ProceduralTerrainSource's own customTerrain_ chaining is
// built on). The real-`.lay`-bytes test requires the real client install to
// be present on this machine (same archive libs/assets' own tests already
// gate on) - skipped, not failed, wherever it isn't; the pure-function tests
// need no archive at all.
#include <doctest/doctest.h>

#include <filesystem>
#include <variant>

#include "assets/TreArchive.h"
#include "terrain/Layer.h"
#include "terrain/TerrainGenerator.h"

using namespace terrain;

namespace {
const char* kRealArchivePath = "C:\\Program Files (x86)\\StarWarsGalaxies\\data_other_00.tre";
} // namespace

TEST_CASE("TerrainGenerator::parseStandalone - real ply_tatt_house_sml_s01.lay parses without "
          "throwing, real structural counts"
          * doctest::skip(!std::filesystem::exists(kRealArchivePath))) {
    assets::TreArchive archive(kRealArchivePath);
    REQUIRE(archive.contains("terrain/ply_tatt_house_sml_s01.lay"));
    auto bytes = archive.extract("terrain/ply_tatt_house_sml_s01.lay");

    auto generator = TerrainGenerator::parseStandalone(bytes);

    // Real, byte-confirmed counts for this specific file (see this
    // feature's plan/status writeup - a real-bytes walk of the raw IFF
    // found exactly one top-level FORM LAYR, one FGRP family, one RGRP
    // family, no MGRP/fractal families).
    CHECK(generator.topLevelLayers.size() == 1);
    CHECK(generator.families.floraFamilies.size() == 1);
    CHECK(generator.families.radialFamilies.size() == 1);
    CHECK(generator.families.fractalFamilies.empty());
}

TEST_CASE("translateLayerBoundaries - shifts every real boundary shape by (dx, dz), recurses into "
          "sub-layers") {
    Layer layer;

    Boundary circleBoundary;
    BoundaryCircle circle;
    circle.centerX = 1.0f;
    circle.centerZ = 2.0f;
    circle.radius = 5.0f;
    circleBoundary.shape = circle;
    layer.boundaries.push_back(circleBoundary);

    Boundary rectBoundary;
    BoundaryRectangle rect;
    rect.x0 = 0.0f;
    rect.y0 = 0.0f;
    rect.x1 = 10.0f;
    rect.y1 = 10.0f;
    rectBoundary.shape = rect;
    layer.boundaries.push_back(rectBoundary);

    Layer subLayer;
    Boundary polyBoundary;
    BoundaryPolygon poly;
    poly.points = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}};
    polyBoundary.shape = poly;
    subLayer.boundaries.push_back(polyBoundary);
    layer.subLayers.push_back(subLayer);

    translateLayerBoundaries(layer, 100.0f, 200.0f);

    const auto& shiftedCircle = std::get<BoundaryCircle>(layer.boundaries[0].shape);
    CHECK(shiftedCircle.centerX == doctest::Approx(101.0f));
    CHECK(shiftedCircle.centerZ == doctest::Approx(202.0f));
    CHECK(shiftedCircle.radius == doctest::Approx(5.0f)); // unaffected by translation

    const auto& shiftedRect = std::get<BoundaryRectangle>(layer.boundaries[1].shape);
    CHECK(shiftedRect.x0 == doctest::Approx(100.0f));
    CHECK(shiftedRect.x1 == doctest::Approx(110.0f));
    CHECK(shiftedRect.y0 == doctest::Approx(200.0f));
    CHECK(shiftedRect.y1 == doctest::Approx(210.0f));

    const auto& shiftedPoly = std::get<BoundaryPolygon>(layer.subLayers[0].boundaries[0].shape);
    CHECK(shiftedPoly.points[0].x == doctest::Approx(100.0f));
    CHECK(shiftedPoly.points[0].y == doctest::Approx(200.0f));
    CHECK(shiftedPoly.points[2].x == doctest::Approx(101.0f));
    CHECK(shiftedPoly.points[2].y == doctest::Approx(201.0f));
}

TEST_CASE("bakeLayerHeight - sets only placeholder (operation==TGO_replace && height==0) affectors, "
          "recurses into sub-layers") {
    Layer layer;

    Affector placeholder;
    AffectorHeightConstant placeholderHeight;
    placeholderHeight.operation = 0; // TGO_replace
    placeholderHeight.height = 0.0f;
    placeholder.affector = placeholderHeight;
    layer.affectors.push_back(placeholder);

    Affector alreadyAuthored;
    AffectorHeightConstant authoredHeight;
    authoredHeight.operation = 0; // TGO_replace
    authoredHeight.height = 7.5f; // non-zero - a real, intentional non-default height
    alreadyAuthored.affector = authoredHeight;
    layer.affectors.push_back(alreadyAuthored);

    Affector nonReplace;
    AffectorHeightConstant addHeight;
    addHeight.operation = 1; // TGO_add - never a placeholder, regardless of height
    addHeight.height = 0.0f;
    nonReplace.affector = addHeight;
    layer.affectors.push_back(nonReplace);

    Layer subLayer;
    Affector subPlaceholder;
    AffectorHeightConstant subPlaceholderHeight;
    subPlaceholderHeight.operation = 0;
    subPlaceholderHeight.height = 0.0f;
    subPlaceholder.affector = subPlaceholderHeight;
    subLayer.affectors.push_back(subPlaceholder);
    layer.subLayers.push_back(subLayer);

    bakeLayerHeight(layer, 42.0f);

    CHECK(std::get<AffectorHeightConstant>(layer.affectors[0].affector).height == doctest::Approx(42.0f));
    CHECK(std::get<AffectorHeightConstant>(layer.affectors[1].affector).height ==
          doctest::Approx(7.5f)); // untouched - already authored
    CHECK(std::get<AffectorHeightConstant>(layer.affectors[2].affector).height ==
          doctest::Approx(0.0f)); // untouched - not a TGO_replace affector
    CHECK(std::get<AffectorHeightConstant>(layer.subLayers[0].affectors[0].affector).height ==
          doctest::Approx(42.0f)); // recursed
}

namespace {

// A minimal, hand-built single-layer generator: an unconditional (no
// boundary - "1.0 if none active", per TerrainGenerator::queryHeight()'s
// own comment) AffectorHeightConstant that replaces height with a fixed
// value - used below to build both a flat "main terrain" generator and a
// circular "building grading" generator without needing any real file.
TerrainGenerator makeFlatHeightGenerator(float height) {
    TerrainGenerator generator;
    Layer layer;
    Affector affector;
    AffectorHeightConstant heightConstant;
    heightConstant.operation = 0; // TGO_replace
    heightConstant.height = height;
    affector.affector = heightConstant;
    layer.affectors.push_back(affector);
    generator.topLevelLayers.push_back(layer);
    return generator;
}

// Same idea, but the AffectorHeightConstant is gated by a real BoundaryCircle
// (feathered, linear falloff) - the actual real shape a `.lay` grading file's
// own flatten layer uses. Real amount formula (TerrainGenerator.cpp's own
// circleIsWithin()): 0 outside `radius` (a hard cutoff - the circle IS the
// outer edge, no falloff extends beyond it), 1.0 inside
// `radius * (1 - featherDistance)`, linearly blended in between -
// `featherDistance` is a FRACTION of radius (e.g. 0.3 = the outer 30% of the
// radius feathers), not an absolute distance.
TerrainGenerator makeCircularFlattenGenerator(float centerX, float centerZ, float radius,
                                                float featherDistanceFraction, float flattenHeight) {
    TerrainGenerator generator;
    Layer layer;

    Boundary boundary;
    BoundaryCircle circle;
    circle.centerX = centerX;
    circle.centerZ = centerZ;
    circle.radius = radius;
    circle.featherFunction = 0; // TGFF_linear
    circle.featherDistance = featherDistanceFraction;
    boundary.shape = circle;
    layer.boundaries.push_back(boundary);

    Affector affector;
    AffectorHeightConstant heightConstant;
    heightConstant.operation = 0; // TGO_replace
    heightConstant.height = flattenHeight;
    affector.affector = heightConstant;
    layer.affectors.push_back(affector);

    generator.topLevelLayers.push_back(layer);
    return generator;
}

} // namespace

TEST_CASE("TerrainGenerator::queryPoint chaining overload - real Core3 sequential-composition "
          "semantics (main generator then a per-building modification generator)") {
    // Mirrors ProceduralTerrainSource's own chaining
    // (main generator_.queryPoint(x,z), then each customTerrain_ entry's
    // queryPoint(x,z,runningPoint)) - exercised directly here since
    // ProceduralTerrainSource itself only exposes construction via parse()
    // on real file bytes, not a way to inject synthetic generators for a
    // unit test.
    TerrainGenerator mainGenerator = makeFlatHeightGenerator(0.0f);
    // radius=10, featherDistance=0.3 -> fully flat (amount=1.0) out to
    // distance 7 (radius * (1-0.3)), linearly feathering to amount=0.0 at
    // distance 10 (radius), hard 0 beyond that.
    TerrainGenerator buildingGenerator = makeCircularFlattenGenerator(
        /*centerX=*/100.0f, /*centerZ=*/200.0f, /*radius=*/10.0f, /*featherDistanceFraction=*/0.3f,
        /*flattenHeight=*/10.0f);

    auto queryChained = [&](float x, float z) {
        TerrainGenerator::Point p = mainGenerator.queryPoint(x, z);
        return buildingGenerator.queryPoint(x, z, p).height;
    };

    // Center of the flatten circle: fully replaced to the grading height.
    CHECK(queryChained(100.0f, 200.0f) == doctest::Approx(10.0f));

    // Within the fully-flat inner radius (distance 5 < 7): still fully
    // replaced.
    CHECK(queryChained(105.0f, 200.0f) == doctest::Approx(10.0f));

    // Outside the circle's own radius entirely (distance 15 > 10): a hard
    // cutoff, amount 0 - the TGO_replace affector doesn't touch the chain
    // at all, stays at the main generator's own flat height.
    CHECK(queryChained(100.0f, 215.0f) == doctest::Approx(0.0f));

    // Partway through the feather ring (distance 8.5, between the inner
    // radius 7 and the outer radius 10): should land strictly between the
    // two heights, not at either extreme.
    float midFeatherHeight = queryChained(108.5f, 200.0f);
    CHECK(midFeatherHeight > 0.0f);
    CHECK(midFeatherHeight < 10.0f);
}
