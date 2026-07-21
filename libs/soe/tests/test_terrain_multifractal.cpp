// Verification for terrain::MultiFractal - ported from the real client's
// sharedFractal/MultiFractal.{h,cpp} (recovered from SWG-Source-ref git
// history, see MultiFractal.cpp's own header comment for provenance).
//
// No independently-known-good oracle exists for this yet (unlike TrnFile's
// real-bytes-pinned header values) - real per-point height samples would
// require either the original game's own dumped values or a from-scratch
// reference reimplementation, neither available. Per the terrain plan,
// this phase's bar is intentionally weaker: determinism, declared [0,1]
// output bounds, and smoothness (adjacent samples don't jump wildly) -
// enough to catch a transcription mistake in the ported algorithm without
// claiming byte-exact parity with the real client's terrain shape.
#include <doctest/doctest.h>

#include <cmath>

#include "terrain/MultiFractal.h"

using namespace terrain;

TEST_CASE("MultiFractal::noise2D - deterministic for a given seed and input") {
    MultiFractalParams params;
    params.seed = 12345;

    MultiFractal a(params);
    MultiFractal b(params);

    for (float x = -500.0f; x <= 500.0f; x += 137.0f) {
        for (float y = -500.0f; y <= 500.0f; y += 211.0f) {
            CHECK(a.noise2D(x, y) == b.noise2D(x, y));
        }
    }
}

TEST_CASE("MultiFractal::noise2D - different seeds produce different fields") {
    MultiFractalParams params;
    params.seed = 1;
    MultiFractal a(params);
    params.seed = 2;
    MultiFractal b(params);

    bool anyDifferent = false;
    for (float x = -1000.0f; x <= 1000.0f; x += 97.0f) {
        for (float y = -1000.0f; y <= 1000.0f; y += 131.0f) {
            if (a.noise2D(x, y) != b.noise2D(x, y)) {
                anyDifferent = true;
            }
        }
    }
    CHECK(anyDifferent);
}

TEST_CASE("MultiFractal::noise2D - stays within [0,1] with no bias/gain") {
    MultiFractalParams params;
    params.seed = 777;

    MultiFractal fractal(params);
    for (float x = -8192.0f; x <= 8192.0f; x += 512.0f) {
        for (float y = -8192.0f; y <= 8192.0f; y += 512.0f) {
            float v = fractal.noise2D(x, y);
            CHECK(v >= 0.0f);
            CHECK(v <= 1.0f);
        }
    }
}

TEST_CASE("MultiFractal::noise2D - adjacent samples are close (smooth, not white noise)") {
    MultiFractalParams params;
    params.seed = 42;
    MultiFractal fractal(params);

    // With the default scale (0.01), a 1-world-unit step is a tiny step in
    // noise space - real Perlin noise should change smoothly, never jump
    // by anywhere near its own full [0,1] range for such a small step.
    for (float x = -200.0f; x <= 200.0f; x += 23.0f) {
        float v0 = fractal.noise2D(x, 0.0f);
        float v1 = fractal.noise2D(x + 1.0f, 0.0f);
        CHECK(std::fabs(v1 - v0) < 0.1f);
    }
}

TEST_CASE("MultiFractal::noise2D - all combination rules produce finite, bounded output") {
    for (auto rule : {FractalCombinationRule::Add, FractalCombinationRule::Multiply,
                       FractalCombinationRule::Crest, FractalCombinationRule::Turbulence,
                       FractalCombinationRule::CrestClamp,
                       FractalCombinationRule::TurbulenceClamp}) {
        MultiFractalParams params;
        params.seed = 99;
        params.combinationRule = rule;
        MultiFractal fractal(params);

        for (float x = -300.0f; x <= 300.0f; x += 61.0f) {
            float v = fractal.noise2D(x, 100.0f);
            CAPTURE(x);
            CHECK(std::isfinite(v));
            CHECK(v >= -0.01f);
            CHECK(v <= 1.01f);
        }
    }
}

TEST_CASE("MultiFractal::noise1D - deterministic and bounded") {
    MultiFractalParams params;
    params.seed = 55;
    MultiFractal fractal(params);

    for (float x = -1000.0f; x <= 1000.0f; x += 83.0f) {
        float v0 = fractal.noise1D(x);
        float v1 = fractal.noise1D(x);
        CHECK(v0 == v1);
        CHECK(v0 >= 0.0f);
        CHECK(v0 <= 1.0f);
    }
}
