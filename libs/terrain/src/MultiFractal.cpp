#include "terrain/MultiFractal.h"

#include <algorithm>
#include <cmath>

namespace terrain {

namespace {

// Ported from sharedRandom/RandomGenerator.cpp's randomNumber() - Numerical
// Recipes in C's ran1() (p.280), the exact PRNG the real engine seeds
// MultiFractal::NoiseGenerator's permutation table with. Only the pieces
// MultiFractal actually uses are ported (setSeed via the constructor,
// random() via next()) - the real class's randomReal()/random(low,high)
// overloads are never called by MultiFractal and are skipped.
class Ran1Generator {
public:
    explicit Ran1Generator(uint32_t seed) : idnum_(-static_cast<int32_t>(seed)) {}

    int32_t next() {
        constexpr int32_t kIM = 2147483647;
        constexpr int32_t kIA = 16807;
        constexpr int32_t kIQ = 127773;
        constexpr int32_t kIR = 2836;
        constexpr int kNTAB = 322;
        // Matches the real NDIV constant exactly, including its float
        // precision loss (kIM-1 is ~2^31, which a 32-bit float can't
        // represent exactly) - that loss is baked into the real algorithm's
        // output, reproduced here rather than "corrected" with double.
        static const float kNDiv = 1.0f + static_cast<float>(kIM - 1) / static_cast<float>(kNTAB);

        int32_t k;
        if (idnum_ <= 0 || iy_ == 0) {
            idnum_ = (-idnum_ < 1) ? 1 : -idnum_;
            for (int j = kNTAB + 7; j >= 0; --j) {
                k = idnum_ / kIQ;
                idnum_ = kIA * (idnum_ - k * kIQ) - kIR * k;
                if (idnum_ < 0) {
                    idnum_ += kIM;
                }
                if (j < kNTAB) {
                    iv_[static_cast<size_t>(j)] = idnum_;
                }
            }
            iy_ = iv_[0];
        }
        k = idnum_ / kIQ;
        idnum_ = kIA * (idnum_ - k * kIQ) - kIR * k;
        if (idnum_ < 0) {
            idnum_ += kIM;
        }
        int j = static_cast<int>(static_cast<float>(iy_) / kNDiv);
        iy_ = iv_[static_cast<size_t>(j)];
        iv_[static_cast<size_t>(j)] = idnum_;
        return iy_;
    }

private:
    int32_t idnum_;
    int32_t iy_ = 0;
    std::array<int32_t, 322> iv_{};
};

inline void normalize2(std::array<float, 2>& v) {
    float s = std::sqrt(v[0] * v[0] + v[1] * v[1]);
    v[0] /= s;
    v[1] /= s;
}

inline float scurve(float t) {
    return (3.0f - (2.0f * t)) * t * t;
}

inline float lerp(float t, float a, float b) {
    return a + t * (b - a);
}

struct PerlinSetup {
    int b0, b1;
    float r0, r1;
};

// Ported from the PERLIN_setup/PERLIN_floor macros. `coord + N` keeps the
// value positive across the tile lookups below; PERLIN_floor converts
// truncation (static_cast<int>) into a true floor for negative inputs.
inline PerlinSetup perlinSetup(float coord) {
    constexpr int kN = 4096;
    constexpr int kBM = 255;

    float t = coord + static_cast<float>(kN);
    int it = static_cast<int>(t);
    int ft = it - ((t < 0.0f && t != static_cast<float>(it)) ? 1 : 0);

    PerlinSetup result;
    result.b0 = ft & kBM;
    result.b1 = (result.b0 + 1) & kBM;
    result.r0 = t - static_cast<float>(ft);
    result.r1 = result.r0 - 1.0f;
    return result;
}

} // namespace

MultiFractal::PerlinNoise::PerlinNoise(uint32_t seed) {
    Ran1Generator rng(seed);

    for (int i = 0; i < kB; ++i) {
        p_[static_cast<size_t>(i)] = i;
        g1_[static_cast<size_t>(i)] =
            static_cast<float>((rng.next() % (kB + kB)) - kB) / static_cast<float>(kB);
        for (int j = 0; j < 2; ++j) {
            g2_[static_cast<size_t>(i)][static_cast<size_t>(j)] =
                static_cast<float>((rng.next() % (kB + kB)) - kB) / static_cast<float>(kB);
        }
        normalize2(g2_[static_cast<size_t>(i)]);
    }

    // Ported as-is from Ken Perlin's original PERLIN.C init(): shuffles by
    // swapping p[i] with p[random() % B] for i counting down from B-1 to 1
    // - a full-range pick, not the "% i" a textbook Fisher-Yates would use.
    // Not a bug to fix; the real client's exact terrain shape depends on
    // reproducing this specific (non-uniform) shuffle.
    for (int i = kB - 1; i >= 1; --i) {
        int k = p_[static_cast<size_t>(i)];
        int j = rng.next() % kB;
        p_[static_cast<size_t>(i)] = p_[static_cast<size_t>(j)];
        p_[static_cast<size_t>(j)] = k;
    }

    for (int i = 0; i < kB + 2; ++i) {
        p_[static_cast<size_t>(kB + i)] = p_[static_cast<size_t>(i)];
        g1_[static_cast<size_t>(kB + i)] = g1_[static_cast<size_t>(i)];
        g2_[static_cast<size_t>(kB + i)] = g2_[static_cast<size_t>(i)];
    }
}

float MultiFractal::PerlinNoise::value(float x) const {
    PerlinSetup s = perlinSetup(x);
    float sx = scurve(s.r0);
    float u = s.r0 * g1_[static_cast<size_t>(p_[static_cast<size_t>(s.b0)])];
    float v = s.r1 * g1_[static_cast<size_t>(p_[static_cast<size_t>(s.b1)])];
    return lerp(sx, u, v);
}

float MultiFractal::PerlinNoise::value(float x, float y) const {
    PerlinSetup sx_ = perlinSetup(x);
    PerlinSetup sy_ = perlinSetup(y);
    float sx = scurve(sx_.r0);
    float sy = scurve(sy_.r0);

    int b00 = p_[static_cast<size_t>(p_[static_cast<size_t>(sx_.b0)] + sy_.b0)];
    int b01 = p_[static_cast<size_t>(p_[static_cast<size_t>(sx_.b0)] + sy_.b1)];
    int b10 = p_[static_cast<size_t>(p_[static_cast<size_t>(sx_.b1)] + sy_.b0)];
    int b11 = p_[static_cast<size_t>(p_[static_cast<size_t>(sx_.b1)] + sy_.b1)];

    auto dot2 = [](float rx, float ry, const std::array<float, 2>& q) {
        return rx * q[0] + ry * q[1];
    };

    float u = dot2(sx_.r0, sy_.r0, g2_[static_cast<size_t>(b00)]);
    float v = dot2(sx_.r1, sy_.r0, g2_[static_cast<size_t>(b10)]);
    float a = lerp(sx, u, v);

    u = dot2(sx_.r0, sy_.r1, g2_[static_cast<size_t>(b01)]);
    v = dot2(sx_.r1, sy_.r1, g2_[static_cast<size_t>(b11)]);
    float b = lerp(sx, u, v);

    return lerp(sy, a, b);
}

namespace {

// bias(b,0)=0, bias(b,0.5)=b, bias(b,1)=1 - ported from MultiFractal.cpp's
// NG_bias/NG_gain (Perlin/Musgrave's standard bias-and-gain functions).
inline float ngBias(float a, float b) {
    return std::pow(a, std::log(b) / std::log(0.5f));
}

inline float ngGain(float a, float b) {
    if (a < 0.001f) {
        return 0.0f;
    }
    if (a > 0.999f) {
        return 1.0f;
    }
    static const float kLogHalf = std::log(0.5f);
    float p = std::log(1.0f - b) / kLogHalf;
    if (a < 0.5f) {
        return std::pow(2.0f * a, p) * 0.5f;
    }
    return 1.0f - std::pow(2.0f * (1.0f - a), p) * 0.5f;
}

} // namespace

MultiFractal::MultiFractal(const MultiFractalParams& params)
    : params_(params), noise_(params.seed), ooTotalAmplitude_(1.0f) {
    float total = 0.0f;
    float amplitude = 1.0f;
    for (int i = 0; i < params_.numberOfOctaves; ++i, amplitude *= params_.amplitude) {
        total += amplitude;
    }
    ooTotalAmplitude_ = 1.0f / total;
}

float MultiFractal::rawNoise1D(float x) const {
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float sum = 0.0f;

    switch (params_.combinationRule) {
        case FractalCombinationRule::Add:
        case FractalCombinationRule::Multiply:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                sum += amplitude * noise_.value(x * frequency);
            }
            if (params_.useSin) {
                sum = std::sin(x + sum);
            }
            return ((sum * ooTotalAmplitude_) + 1.0f) * 0.5f;

        case FractalCombinationRule::Crest:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                sum += amplitude * (1.0f - std::fabs(noise_.value(x * frequency)));
            }
            break;

        case FractalCombinationRule::Turbulence:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                sum += amplitude * std::fabs(noise_.value(x * frequency));
            }
            break;

        case FractalCombinationRule::CrestClamp:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                float n = noise_.value(x * frequency);
                sum += amplitude * (1.0f - std::clamp(n, 0.0f, 1.0f));
            }
            break;

        case FractalCombinationRule::TurbulenceClamp:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                float n = noise_.value(x * frequency);
                sum += amplitude * std::clamp(n, 0.0f, 1.0f);
            }
            break;
    }

    if (params_.useSin) {
        sum = std::sin(x + sum);
    }
    return sum * ooTotalAmplitude_;
}

float MultiFractal::rawNoise2D(float x, float y) const {
    float frequency = 1.0f;
    float amplitude = 1.0f;
    float sum = 0.0f;

    switch (params_.combinationRule) {
        case FractalCombinationRule::Add:
        case FractalCombinationRule::Multiply:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                sum += amplitude * noise_.value(x * frequency, y * frequency);
            }
            // Matches the real getValueAdd_2 exactly: the sin modifier only
            // ever folds in `x`, never `y` - a confirmed real-client quirk,
            // not a transcription slip. Never observable in practice since
            // useSin is never set by any real parsed file.
            if (params_.useSin) {
                sum = std::sin(x + sum);
            }
            return ((sum * ooTotalAmplitude_) + 1.0f) * 0.5f;

        case FractalCombinationRule::Crest:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                sum += amplitude * (1.0f - std::fabs(noise_.value(x * frequency, y * frequency)));
            }
            break;

        case FractalCombinationRule::Turbulence:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                sum += amplitude * std::fabs(noise_.value(x * frequency, y * frequency));
            }
            break;

        case FractalCombinationRule::CrestClamp:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                float n = noise_.value(x * frequency, y * frequency);
                sum += amplitude * (1.0f - std::clamp(n, 0.0f, 1.0f));
            }
            break;

        case FractalCombinationRule::TurbulenceClamp:
            for (int i = 0; i < params_.numberOfOctaves;
                 ++i, frequency *= params_.frequency, amplitude *= params_.amplitude) {
                float n = noise_.value(x * frequency, y * frequency);
                sum += amplitude * std::clamp(n, 0.0f, 1.0f);
            }
            break;
    }

    if (params_.useSin) {
        sum = std::sin(x + sum);
    }
    return sum * ooTotalAmplitude_;
}

float MultiFractal::noise1D(float worldX) const {
    float x = worldX * params_.scaleX + params_.offsetX;
    float result = rawNoise1D(x);
    if (params_.useBias) {
        result = ngBias(result, params_.bias);
    }
    if (params_.useGain) {
        result = ngGain(result, params_.gain);
    }
    return result;
}

float MultiFractal::noise2D(float worldX, float worldZ) const {
    float x = worldX * params_.scaleX + params_.offsetX;
    float y = worldZ * params_.scaleY + params_.offsetY;
    float result = rawNoise2D(x, y);
    if (params_.useBias) {
        result = ngBias(result, params_.bias);
    }
    if (params_.useGain) {
        result = ngGain(result, params_.gain);
    }
    return result;
}

} // namespace terrain
