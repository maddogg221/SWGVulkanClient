#pragma once

#include <array>
#include <cstdint>

namespace terrain {

// Ported from the real client's sharedFractal/MultiFractal.{h,cpp} (SOE,
// copyright 2001) - recovered from C:\SWG-Source-ref's git history (commit
// 9f0f8a80 "Added sharedFractal library"; the library was deleted from the
// working tree in a later "remove windows sources" pass but is still fully
// present in history and nowhere else). Combines several octaves of Ken
// Perlin's reference gradient noise (his original PERLIN.C, c++-ified by
// SOE) into a single value, matching every real .trn file's FractalGroup/
// BitmapGroup family entries (wire format FORM MFRC, confirmed against the
// same source drop's MultiFractalReaderWriter.cpp - seed/bias/gain/
// octaves/frequency/amplitude/scaleX/scaleY/[offsetX/offsetY]/
// combinationRule, in that order) that drive AffectorHeightFractal and the
// color-ramp-fractal affectors.
//
// The exact permutation table a given seed produces IS the real terrain
// shape, so this is a byte-for-byte port of both the noise algorithm and
// the specific PRNG that seeds it (Numerical Recipes in C's ran1(), as
// adapted by the real engine's sharedRandom/RandomGenerator.cpp - same git
// commit history, recovered from commit 56cf409d) - not a "conceptually
// equivalent" reimplementation. Even the PRNG's own internal float-division
// index (NDIV, involving a value near 2^31 losing precision in a 32-bit
// float) is reproduced as float arithmetic on purpose: that precision loss
// is baked into the original algorithm's output, not a bug to "fix".
//
// Deliberately simplified vs. the original: the original MultiFractal is a
// heavily mutable class (get/set every parameter, copy/assignment, a
// chunk-grid-shaped value cache) built for a live in-engine terrain editor.
// This project only ever constructs one from already-parsed file data and
// then queries it, so this port is immutable-after-construction with no
// setters, no cache, and no getValue2()/getValueCache()/getValueCache2()
// (getValue2's function-pointer dispatch is confirmed byte-for-byte
// equivalent to getValue's inline switch - just a different dispatch
// mechanism for the identical math - so only one code path is kept).
enum class FractalCombinationRule {
    Add,
    // Real engine quirk, not a port mistake: MultiFractal::setCombinationRule
    // maps CR_multiply to the exact same function pointer as CR_add
    // (getValueAdd_1/getValueAdd_2) - there is no distinct multiply
    // algorithm in the real client. Preserved as-is.
    Multiply,
    Crest,
    Turbulence,
    CrestClamp,
    TurbulenceClamp,
};

struct MultiFractalParams {
    uint32_t seed = 0;
    float scaleX = 0.01f;
    float scaleY = 0.01f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    int numberOfOctaves = 2;
    float frequency = 4.0f;
    float amplitude = 0.5f;
    bool useBias = false;
    float bias = 0.5f;
    bool useGain = false;
    float gain = 0.7f;
    // Confirmed dead for any real parsed file: MultiFractalReaderWriter
    // never reads/writes this field for either wire version (0000/0001),
    // so every real .trn-sourced MultiFractal has this false. Kept only
    // because the real class exposes it and the math is trivial to port
    // faithfully alongside everything else.
    bool useSin = false;
    FractalCombinationRule combinationRule = FractalCombinationRule::Add;
};

class MultiFractal {
public:
    explicit MultiFractal(const MultiFractalParams& params);

    // Both return a value nominally in [0,1] (the real class's own
    // documented contract, enforced there by a DEBUG_FATAL this port
    // doesn't replicate) - real bias/gain post-processing can occasionally
    // nudge slightly outside that range via pow()'s edge behavior, so
    // callers that need a hard guarantee should clamp themselves.
    float noise1D(float worldX) const;
    float noise2D(float worldX, float worldZ) const;

private:
    // Ported from MultiFractal::NoiseGenerator - Ken Perlin's reference
    // noise implementation. Returns raw noise in [-1,1]; MultiFractal's
    // combination-rule loop (Add/Crest/Turbulence/...) turns several
    // octaves of this into the final [0,1] value.
    class PerlinNoise {
    public:
        explicit PerlinNoise(uint32_t seed);

        float value(float x) const;
        float value(float x, float y) const;

    private:
        static constexpr int kB = 256;

        std::array<int, kB + kB + 2> p_{};
        std::array<std::array<float, 2>, kB + kB + 2> g2_{};
        std::array<float, kB + kB + 2> g1_{};
    };

    float rawNoise1D(float x) const;
    float rawNoise2D(float x, float y) const;

    MultiFractalParams params_;
    PerlinNoise noise_;
    float ooTotalAmplitude_;
};

} // namespace terrain
