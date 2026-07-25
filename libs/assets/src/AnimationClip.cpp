#include "assets/AnimationClip.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

#include "assets/IffReader.h"

namespace assets {

namespace {
constexpr uint32_t kCkatTag = 0x434B4154; // 'CKAT'
constexpr uint32_t kXfrmTag = 0x5846524D; // 'XFRM'
constexpr uint32_t kXfinTag = 0x5846494E; // 'XFIN'
constexpr uint32_t kArotTag = 0x41524F54; // 'AROT'
constexpr uint32_t kQchnTag = 0x5143484E; // 'QCHN'
constexpr uint32_t kAtrnTag = 0x4154524E; // 'ATRN'
constexpr uint32_t kChnlTag = 0x43484E4C; // 'CHNL'
constexpr uint32_t kLoctTag = 0x4C4F4354; // 'LOCT'

// Real, live-extracted per-context-byte lookup table (x32dbg static-memory
// dump of swgemu.exe:01945BB8, 2026-07-22 morning session). Each real
// per-component "context byte" (see parseQchn's own comment - the 3-byte
// per-channel value this project reads once per QCHN chunk) maps to a real
// BASE offset and a real precision LEVEL (0-6) - together with
// kScaleTable below, these are exactly what the real client's own
// AC0AD0/AC0B30 decode functions use to reconstruct each retained
// component: `value = base +/- magnitude * scale(level)`. Only covers the
// byte range actually observed across real files this session (0x9d-0xfd)
// - see lookupBaseTableEntry()'s own comment for the out-of-range
// fallback. The `level` values form a clean binary-subdivision pattern
// (level 6 = finest/most entries, level 1 = coarsest/fewest, confirmed by
// eye against the real extracted floats) - a real ADAPTIVE quantization
// scheme, picking a coarse region (base) and a fitting precision (level)
// per component per channel, not a single fixed range like this project's
// original guessed variants 0/1 assumed.
struct BaseTableEntry {
    uint8_t byte;
    float base;
    int level;
};
constexpr BaseTableEntry kBaseTable[] = {
    {0x9d, -0.076923072f, 6}, {0x9e, -0.046153843f, 6}, {0x9f, -0.015384614f, 6},
    {0xa0, 0.015384674f, 6},  {0xa1, 0.046153903f, 6},  {0xa2, 0.076923132f, 6},
    {0xa3, 0.107692361f, 6},  {0xa4, 0.138461590f, 6},  {0xa5, 0.169230819f, 6},
    {0xa6, 0.200000048f, 6},  {0xa7, 0.230769277f, 6},  {0xa8, 0.261538506f, 6},
    {0xa9, 0.292307734f, 6},  {0xaa, 0.323076963f, 6},  {0xab, 0.353846192f, 6},
    {0xac, 0.384615421f, 6},  {0xad, 0.415384650f, 6},  {0xae, 0.446153879f, 6},
    {0xaf, 0.476923108f, 6},  {0xb0, 0.507692337f, 6},  {0xb1, 0.538461566f, 6},
    {0xb2, 0.569230795f, 6},  {0xb3, 0.600000024f, 6},  {0xb4, 0.630769253f, 6},
    {0xb5, 0.661538482f, 6},  {0xb6, 0.692307711f, 6},  {0xb7, 0.723076940f, 6},
    {0xb8, 0.753846169f, 6},  {0xb9, 0.784615397f, 6},  {0xba, 0.815384626f, 6},
    {0xbb, 0.846153855f, 6},  {0xbc, 0.876923084f, 6},  {0xbd, 0.907692313f, 6},
    {0xbe, 0.938461542f, 6},  {0xbf, 0.969230771f, 6},  {0xc0, -0.939393938f, 5},
    {0xc1, -0.878787875f, 5}, {0xc2, -0.818181813f, 5}, {0xc3, -0.757575750f, 5},
    {0xc4, -0.696969688f, 5}, {0xc5, -0.636363626f, 5}, {0xc6, -0.575757563f, 5},
    {0xc7, -0.515151501f, 5}, {0xc8, -0.454545438f, 5}, {0xc9, -0.393939376f, 5},
    {0xca, -0.333333313f, 5}, {0xcb, -0.272727251f, 5}, {0xcc, -0.212121189f, 5},
    {0xcd, -0.151515126f, 5}, {0xce, -0.090909064f, 5}, {0xcf, -0.030303001f, 5},
    {0xd0, 0.030303001f, 5},  {0xd1, 0.090909123f, 5},  {0xd2, 0.151515245f, 5},
    {0xd3, 0.212121248f, 5},  {0xd4, 0.272727251f, 5},  {0xd5, 0.333333373f, 5},
    {0xd6, 0.393939495f, 5},  {0xd7, 0.454545498f, 5},  {0xd8, 0.515151501f, 5},
    {0xd9, 0.575757623f, 5},  {0xda, 0.636363745f, 5},  {0xdb, 0.696969748f, 5},
    {0xdc, 0.757575750f, 5},  {0xdd, 0.818181872f, 5},  {0xde, 0.878787994f, 5},
    {0xdf, 0.939393997f, 5},  {0xe0, -0.882352948f, 4}, {0xe1, -0.764705896f, 4},
    {0xe2, -0.647058845f, 4}, {0xe3, -0.529411793f, 4}, {0xe4, -0.411764681f, 4},
    {0xe5, -0.294117630f, 4}, {0xe6, -0.176470578f, 4}, {0xe7, -0.058823526f, 4},
    {0xe8, 0.058823586f, 4},  {0xe9, 0.176470637f, 4},  {0xea, 0.294117689f, 4},
    {0xeb, 0.411764741f, 4},  {0xec, 0.529411793f, 4},  {0xed, 0.647058845f, 4},
    {0xee, 0.764705896f, 4},  {0xef, 0.882352948f, 4},  {0xf0, -0.777777791f, 3},
    {0xf1, -0.555555582f, 3}, {0xf2, -0.333333313f, 3}, {0xf3, -0.111111104f, 3},
    {0xf4, 0.111111164f, 3},  {0xf5, 0.333333373f, 3},  {0xf6, 0.555555582f, 3},
    {0xf7, 0.777777791f, 3},  {0xf8, -0.600000024f, 2}, {0xf9, -0.199999988f, 2},
    {0xfa, 0.200000048f, 2},  {0xfb, 0.600000024f, 2},  {0xfc, -0.333333313f, 1},
    {0xfd, 0.333333373f, 1},
};

// Real, live-extracted per-precision-level scale constants (x32dbg
// static-memory dump of swgemu.exe:018AF650, same session). Index 0-6
// matches kBaseTable's own `level` field. `scaleC` is the real scale used
// for the "low" field (10 bits total: 1 sign + 9 magnitude -
// swgemu.exe:AC0AD0); `scaleAB` is the real scale used for the "top"/
// "middle" fields (11 bits total each: 1 sign + 10 magnitude -
// swgemu.exe:AC0B30). The two real tables sit 8 bytes apart in memory
// (018AF650 vs 018AF658 - exactly one float's worth), confirming they're
// really one combined table, not two independent ones.
struct ScaleTableEntry {
    float scaleC;
    float scaleAB;
};
constexpr ScaleTableEntry kScaleTable[7] = {
    {0.000977517f, 0.00195695f},  {0.000651678f, 0.00130463f}, {0.000391007f, 0.000782779f},
    {0.000217226f, 0.000434877f}, {0.000115002f, 0.000230229f}, {0.0000592435f, 0.000118603f},
    {0.0000300774f, 0.0000602138f},
};

// Looks up a real per-component context byte's (base, level) pair. Falls
// back to the nearest table entry (clamping) for any byte outside the
// real observed 0x9d-0xfd range - every real file checked this session
// only ever used bytes in that range, so an out-of-range byte here would
// mean genuinely new real data this table doesn't cover yet, not a real
// expected case.
BaseTableEntry lookupBaseTableEntry(uint8_t byte) {
    constexpr size_t kCount = sizeof(kBaseTable) / sizeof(kBaseTable[0]);
    if (byte <= kBaseTable[0].byte) return kBaseTable[0];
    if (byte >= kBaseTable[kCount - 1].byte) return kBaseTable[kCount - 1];
    for (size_t i = 0; i < kCount; ++i) {
        if (kBaseTable[i].byte == byte) return kBaseTable[i];
    }
    return kBaseTable[0]; // unreachable given the table is byte-contiguous
}

// Real per-keyframe rotation payload decode ("smallest three" quantized
// quaternion). Phase 21 live x32dbg reverse-engineering of the real,
// official SWGEmu.exe client (see project memory - traced from the real
// "SkeletalAnimationTemplate"/"CompressedKeyframeAnimation" string
// landmarks all the way down to real FPU instructions around
// swgemu.exe:00AC0DE3-00AC0E0F) found the real client's own reconstruction
// of the dropped (4th, omitted) quaternion component: it computes
// `sqrt(1 - x*x - y*y - z*z)` via a bare `fsqrt` and stores it directly -
// UNCONDITIONALLY POSITIVE, no sign check, no candidate selection anywhere
// in the real decode path. This directly refutes this project's earlier
// (same phase, same session) assumption - built on general "smallest
// three"/Granny3D background research rather than the real client's own
// code - that the dropped component's sign is real, encoder-intended
// information requiring disambiguation. That wrong assumption had driven an
// entire two-state Viterbi DP pass (picking whichever of two candidate
// signs minimized total angular change across a channel) plus a
// freeze-on-anomaly mitigation for the cases the DP still got wrong - both
// now removed entirely. The real fix is to never guess a sign at all,
// matching the real client exactly: the encoder guarantees the dropped
// component's sign in advance (by negating the whole quaternion, valid
// since `q` and `-q` are the same rotation, whenever needed so the dropped
// component comes out non-negative), so the decoder never needs to.
int g_bitLayoutVariant = 2; // see AnimationClip::setBitLayoutVariantForTesting() - 2 is the confirmed-real layout

// Phase 21 live experiment: the Z-negation below was found and validated
// using LEG motion specifically (a real live A/B test on a real leg swing) -
// never independently verified for arms, which have been the single most
// consistently flagged bone group in every diagnostic tonight, and were
// confirmed live (via a real RenderDoc GPU capture) to be the very first
// submesh that renders malformed, before any other body part. Testing
// whether arms need a DIFFERENT real axis convention than legs/spine (not
// implausible for an old, hand-authored skeleton). true = skip the
// Z-negation for arm-chain bones only.
bool g_experimentSkipZNegationForArms = false;

// Phase 21 live experiment: `root` is architecturally unlike every other
// bone - it has no parent, so its decoded rotation directly IS the whole
// body's world orientation rather than one joint's contribution buried in a
// chain. Live evidence (self flipping/twisting during real WASD movement,
// worsening the longer the walk clip loops, but perfectly stable with
// animation disabled via the 'B' bind-pose debug key) isolates the problem
// to the walk clip's decoded rotation data specifically, and root is the
// one bone whose own error would visibly rotate the ENTIRE body rather than
// just one limb - the same Z-negation convention question already found
// real for arms, now being tested for root specifically. true = skip the
// Z-negation for the `root` bone only.
bool g_experimentSkipZNegationForRoot = false;

// Phase 21 live experiment (2026-07-25) - direct user report from a real
// walk cycle: the legs visibly CROSS during walking (one leg swinging
// toward/through the other's side) rather than staying on their own side -
// a real, distinctive signature of a lateral/handedness sign error, the
// same class of bug already found and fixed for the wrist/finger chain
// this session (Z-negation needed SKIPPED there, unlike every other
// bone). Testing the identical fix for the leg chain. true = skip the
// Z-negation for leg-chain bones only.
bool g_experimentSkipZNegationForLegs = false;

bool isArmChainBone(const std::string& boneName) {
    // Phase 21 live experiment: tried extending this to include finger
    // bones (thumb/index/ring), on the theory that they need the same
    // Z-negation treatment arms already confirmed they need - live result
    // was NEGATIVE: toggling it flipped the hand artifact's direction
    // rather than fixing it, meaning both "negate" and "don't negate" are
    // wrong for fingers, just wrong in different directions. Reverted -
    // fingers are NOT simply an extension of the arm-chain Z-negation
    // fix; whatever's actually wrong with them is something else. Kept
    // scoped to the originally-confirmed arm/forearm/ulna/wrist bones only.
    static const char* kArmBones[] = {"larm",     "lforearm", "lulna",    "lwrist",
                                       "rarm",     "rforearm", "rulna",    "rwrist"};
    for (const char* name : kArmBones) {
        if (boneName.size() == std::strlen(name)) {
            bool match = true;
            for (size_t i = 0; i < boneName.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(boneName[i])) != name[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

bool isRootBone(const std::string& boneName) {
    static const char kRoot[] = "root";
    if (boneName.size() != std::strlen(kRoot)) return false;
    for (size_t i = 0; i < boneName.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(boneName[i])) != kRoot[i]) return false;
    }
    return true;
}

// Phase 21 live experiment: variant 2's "always drop w" rule (see
// decodeSmallestThreeQuaternion's own comment - no per-keyframe/per-channel
// dropped-axis index was found stored anywhere in the real client) fixed
// the whole-body orientation and most limbs live, but legs specifically
// still show real, live-confirmed artifacts (flat "sail" triangles,
// twisting) - suggesting legs may genuinely drop a DIFFERENT axis than w.
// -1 = disabled (use the normal always-w rule for every bone, including
// legs); 0/1/2/3 = force leg-chain bones specifically to drop that axis
// instead, to empirically find the real one without needing more RE.
int g_legForcedDropIndex = -1;

// Phase 21 live experiment: same idea as g_legForcedDropIndex, now for
// finger bones specifically. Extending the arm-chain Z-negation fix to
// fingers gave a NEGATIVE result (flipped the artifact's direction rather
// than fixing it - see isArmChainBone's own comment), which rules out a
// simple sign issue but doesn't rule out fingers needing a different
// FIXED dropped axis than the normal always-w rule, the same open
// question legs raised before the real base/scale tables were extracted.
// -1 = disabled (normal always-w rule); 0/1/2/3 = force finger bones to
// drop that axis instead.
int g_fingerForcedDropIndex = -1;

// Widened 2026-07-23: user confirmed live that the elbow-through-wrist
// bones show the exact same collapse artifact as the fingers, not a
// separate bug - despite lforearm/lulna/lwrist already being in
// isArmChainBone's Z-negation group above, that fix alone doesn't resolve
// them, the same way it didn't resolve fingers. Reusing this SAME override
// mechanism (rather than a separate one) so the existing 'G'/'H' live
// debug keys can A/B-test candidate fixes across the whole broken chain at
// once.
bool isFingerChainBone(const std::string& boneName) {
    static const char* kFingerBones[] = {"lthumb01", "lthumb02", "lindex01", "lindex02",
                                          "lring01",  "lring02",  "rthumb01", "rthumb02",
                                          "rindex01", "rindex02", "rring01",  "rring02",
                                          "lforearm", "lulna",    "lwrist",
                                          "rforearm", "rulna",    "rwrist"};
    for (const char* name : kFingerBones) {
        if (boneName.size() == std::strlen(name)) {
            bool match = true;
            for (size_t i = 0; i < boneName.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(boneName[i])) != name[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

bool isLegChainBone(const std::string& boneName) {
    static const char* kLegBones[] = {"lthigh", "lshin", "lankle", "ltoe",
                                       "rthigh", "rshin", "rankle", "rtoe"};
    for (const char* name : kLegBones) {
        if (boneName.size() == std::strlen(name)) {
            bool match = true;
            for (size_t i = 0; i < boneName.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(boneName[i])) != name[i]) {
                    match = false;
                    break;
                }
            }
            if (match) return true;
        }
    }
    return false;
}

Quaternion decodeSmallestThreeQuaternion(uint32_t u32, bool skipZNegation, int forcedIdxOverride,
                                          uint8_t ctxByteA, uint8_t ctxByteB, uint8_t ctxByteC) {
    constexpr float kScale = 0.70710678f; // 1/sqrt(2) - max magnitude of a non-largest quaternion component
    uint32_t idx;
    uint32_t f0, f1, f2;
    float vals[3];
    if (g_bitLayoutVariant == 2) {
        // Real, CONFIRMED layout (live x32dbg RE of the official client,
        // 2026-07-22 morning session - traced the real per-channel loader
        // all the way from the real "CKAT"/"0001"/"INFO"/"XFRM"/"XFIN"/
        // "AROT"/"QCHN" tag reads down to the real per-component field
        // decode). Two things variants 0/1 above both got wrong:
        // 1. There is NO stored "which axis was dropped" index anywhere -
        //    not in this 32-bit value (11+11+10 = 32 bits exactly, no room
        //    left for one) and not in the real per-channel 3-byte header
        //    either (confirmed live: that header is 3 SEPARATE bytes, one
        //    per retained component, each selecting a per-component
        //    scale-table row - not an index at all, and this project's own
        //    QCHN parser already reads and discards these 3 bytes
        //    correctly, byte-count-wise). The dropped component is always
        //    the 4th (w) - the simplest, most common "smallest three"
        //    convention, consistent with never finding anywhere else the
        //    real client could be storing a per-keyframe/per-channel
        //    choice.
        // 2. The three retained fields are NOT uniform 10-bit two's-
        //    complement/centered values - they're an ASYMMETRIC
        //    11/11/10-bit SIGNED-MAGNITUDE layout: the real client
        //    (swgemu.exe:AC0AD0/AC0B30) tests each field's own TOP bit as
        //    an explicit sign flag, then interprets the REMAINING bits as
        //    an unsigned magnitude multiplied by a real per-component
        //    scale value and added to or subtracted from a real
        //    per-component base value - both looked up per-component from
        //    kBaseTable/kScaleTable above using the real per-channel
        //    context bytes (ctxByteA/B/C), extracted directly from
        //    swgemu.exe's own memory this session, not approximated.
        // Phase 21 live experiment: forcedIdxOverride (see
        // g_legForcedDropIndex's own comment) lets a specific bone group
        // (currently leg-chain bones) test a DIFFERENT fixed dropped axis
        // than the normal always-w rule, since live evidence shows legs
        // still breaking under always-w even though most other bones look
        // correct with it.
        idx = forcedIdxOverride >= 0 ? static_cast<uint32_t>(forcedIdxOverride) : 3u;
        uint32_t fieldA = (u32 >> 21) & 0x7FFu; // bits 31-21, 11 bits (1 sign + 10 magnitude)
        uint32_t fieldB = (u32 >> 10) & 0x7FFu; // bits 20-10, 11 bits (1 sign + 10 magnitude)
        uint32_t fieldC = u32 & 0x3FFu;         // bits 9-0, 10 bits (1 sign + 9 magnitude)
        auto decodeField11 = [](uint32_t field, uint8_t ctxByte) {
            BaseTableEntry entry = lookupBaseTableEntry(ctxByte);
            float scale = kScaleTable[entry.level].scaleAB;
            bool negative = (field & 0x400u) != 0; // bit 10 (top bit of an 11-bit field)
            uint32_t magnitude = field & 0x3FFu;    // bits 0-9, max 1023
            float delta = static_cast<float>(magnitude) * scale;
            return negative ? (entry.base - delta) : (entry.base + delta);
        };
        auto decodeField10 = [](uint32_t field, uint8_t ctxByte) {
            BaseTableEntry entry = lookupBaseTableEntry(ctxByte);
            float scale = kScaleTable[entry.level].scaleC;
            bool negative = (field & 0x200u) != 0; // bit 9 (top bit of a 10-bit field)
            uint32_t magnitude = field & 0x1FFu;    // bits 0-8, max 511
            float delta = static_cast<float>(magnitude) * scale;
            return negative ? (entry.base - delta) : (entry.base + delta);
        };
        vals[0] = decodeField11(fieldA, ctxByteA);
        vals[1] = decodeField11(fieldB, ctxByteB);
        vals[2] = decodeField10(fieldC, ctxByteC);
    } else if (g_bitLayoutVariant == 1) {
        // Variant 1: idx in the bottom 2 bits, signed (two's complement)
        // fields - edged out variant 0 by ~0.01% aggregate real validity
        // across a 75k-keyframe, 57-file real sample (see
        // AnimationClip.h's own comment) - close enough that it needs a
        // real live visual A/B, not just the validity metric, to decide.
        idx = u32 & 0x3u;
        f0 = (u32 >> 2) & 0x3FFu;
        f1 = (u32 >> 12) & 0x3FFu;
        f2 = (u32 >> 22) & 0x3FFu;
        auto convSigned = [](uint32_t field) {
            int32_t signedField = field >= 512 ? static_cast<int32_t>(field) - 1024 : static_cast<int32_t>(field);
            return static_cast<float>(signedField) / 512.0f * kScale;
        };
        vals[0] = convSigned(f0);
        vals[1] = convSigned(f1);
        vals[2] = convSigned(f2);
    } else {
        idx = (u32 >> 30) & 0x3u;
        f0 = u32 & 0x3FFu;
        f1 = (u32 >> 10) & 0x3FFu;
        f2 = (u32 >> 20) & 0x3FFu;
        auto conv = [](uint32_t field) {
            return (static_cast<float>(field) - 511.5f) / 511.5f * kScale;
        };
        vals[0] = conv(f0);
        vals[1] = conv(f1);
        vals[2] = conv(f2);
    }
    float sumSq = vals[0] * vals[0] + vals[1] * vals[1] + vals[2] * vals[2];
    // Matches the real client exactly (see the header comment above this
    // function): the dropped component is always the non-negative square
    // root, clamped to 0 for the same real quantization-boundary frames
    // where sumSq can drift slightly above 1.0.
    float dropped = sumSq < 1.0f ? std::sqrt(1.0f - sumSq) : 0.0f;

    float comps[4];
    int src = 0;
    for (uint32_t i = 0; i < 4; ++i) {
        comps[i] = (i == idx) ? dropped : vals[src++];
    }
    // Real bug found live (Phase 21): for the real (~2-6%,
    // quantization-boundary) frames where sumSq > 1.0, `dropped` clamps to
    // 0.0f rather than the real quaternion's true (small negative-margin)
    // value, leaving the reconstructed quaternion's length slightly above 1
    // (measured live up to ~1.19x). A non-unit quaternion fed into a
    // rotation-matrix conversion silently leaks a scale/shear component
    // that COMPOUNDS multiplicatively down a long bone chain - renormalize
    // unconditionally so every returned quaternion is unit-length
    // regardless of which frames hit the clamped case.
    float lenSq = comps[0] * comps[0] + comps[1] * comps[1] + comps[2] * comps[2] + comps[3] * comps[3];
    if (lenSq > 1e-8f) {
        float invLen = 1.0f / std::sqrt(lenSq);
        comps[0] *= invLen;
        comps[1] *= invLen;
        comps[2] *= invLen;
        comps[3] *= invLen;
    }
    // Real bug found live (Phase 21): the real per-keyframe rotation
    // payload's Z component needs negating to match DirectXMath's
    // quaternion convention - confirmed via a real live A/B test (self
    // visibly reconstructing a recognizable leg+foot shape only once this
    // negation was applied; bind pose, which never touches this decoded
    // data, was already correct without it, isolating the mismatch to
    // specifically how `.ans` rotation data relates to our convention).
    // Likely a real coordinate-handedness difference between SWG's engine
    // (which licensed RAD Game Tools' Granny3D for skeletal animation) and
    // DirectXMath - not fully explained yet, kept live-tunable via
    // Visualizer.cpp's own 'Z' key debug toggle in case a different
    // bone/clip combination shows this needs revisiting.
    Quaternion q;
    q.x = comps[0];
    q.y = comps[1];
    q.z = skipZNegation ? comps[2] : -comps[2];
    q.w = comps[3];
    return q;
}

// A real CHNK QCHN: leading uint16 keyframe count, then a real 3-byte
// per-channel value - confirmed live (x32dbg, 2026-07-22 morning session)
// to be 3 SEPARATE bytes, one per retained rotation component, each a
// real "context byte" selecting that component's base/level from
// kBaseTable above (see decodeSmallestThreeQuaternion's own comment) -
// then `count` real keyframe records, each [uint16 frame][uint32 LE
// quantized rotation payload, see decodeSmallestThreeQuaternion above].
std::vector<QuaternionKeyframe> parseQchn(const IffChunk& chunk, const std::string& boneNameForDebug) {
    soe::PacketBuffer buf = chunk.data;
    buf.resetReadCursor();
    uint16_t rawCount = buf.readUint16();
    std::vector<uint8_t> contextBytes = buf.readBytes(3);
    size_t count = rawCount;

    // Phase 21 diagnostic (2026-07-24, code audit) - check whether this
    // specific bone's real per-channel context bytes actually fall within
    // the confirmed-observed 0x9d-0xfd table range, or silently clamp to a
    // wrong base/level via lookupBaseTableEntry's fallback - never directly
    // verified for the wrist/finger chain specifically, only for a general
    // sample in an earlier session.
    if (isFingerChainBone(boneNameForDebug)) {
        auto describe = [](uint8_t b) {
            return (b < 0x9d || b > 0xfd) ? "OUT-OF-RANGE-CLAMPED" : "in-range";
        };
        std::printf("[ANIMDBG] QCHN context bytes bone=%s: A=0x%02x(%s) B=0x%02x(%s) C=0x%02x(%s) count=%u\n",
                    boneNameForDebug.c_str(), contextBytes[0], describe(contextBytes[0]), contextBytes[1],
                    describe(contextBytes[1]), contextBytes[2], describe(contextBytes[2]), rawCount);
    }

    // Real fix, live-verified 2026-07-25 (post quaternion-byte-order-fix
    // retest of the finger 'H' axis-fix debug key, previously tested with a
    // negative result under the OLD, byte-order-broken skeleton parser -
    // that old result no longer applies): the wrist/finger chain (this
    // project's own isFingerChainBone list, already widened to include
    // forearm/ulna/wrist) needs Z-negation SKIPPED, unlike every other bone
    // - direct live A/B against a live private test server confirmed
    // fingers curl the correct direction (into the palm, not away from it)
    // only once this is applied. Now the permanent default rather than a
    // manual debug-key toggle.
    bool skipZNegation = (g_experimentSkipZNegationForArms && isArmChainBone(boneNameForDebug)) ||
                          (g_experimentSkipZNegationForRoot && isRootBone(boneNameForDebug)) ||
                          (g_experimentSkipZNegationForLegs && isLegChainBone(boneNameForDebug)) ||
                          isFingerChainBone(boneNameForDebug);
    int forcedIdxOverride = -1;
    if (g_legForcedDropIndex >= 0 && isLegChainBone(boneNameForDebug)) {
        forcedIdxOverride = g_legForcedDropIndex;
    } else if (g_fingerForcedDropIndex >= 0 && isFingerChainBone(boneNameForDebug)) {
        forcedIdxOverride = g_fingerForcedDropIndex;
    }
    std::vector<QuaternionKeyframe> result(count);
    for (size_t i = 0; i < count; ++i) {
        result[i].frame = buf.readUint16();
        result[i].rotation = decodeSmallestThreeQuaternion(buf.readUint32(), skipZNegation, forcedIdxOverride,
                                                             contextBytes[0], contextBytes[1], contextBytes[2]);
    }
    // Phase 21 diagnostic (2026-07-24, code audit) - last keyframe's frame
    // number, not just the count, for chain bones - independent per-channel
    // looping (see sampleRotationChannel's own period = lastFrame+1) means
    // two channels with different total durations would slowly drift out of
    // phase with each other over time even if each is individually sane.
    if (isFingerChainBone(boneNameForDebug) && !result.empty()) {
        std::printf("[ANIMDBG] QCHN bone=%s count=%zu lastFrame=%u\n", boneNameForDebug.c_str(), count,
                    result.back().frame);
    }
    return result;
}

// A real CHNK CHNL: leading uint16 keyframe count (no base prefix, unlike
// QCHN), then `count` real keyframe records, each [uint16 frame][float32
// LE value] - confirmed via real per-axis curves that are smooth/monotonic
// when read this way (a real root-bone translation curve during a real
// prone-to-standing clip).
std::vector<ScalarKeyframe> parseChnl(const IffChunk& chunk) {
    soe::PacketBuffer buf = chunk.data;
    buf.resetReadCursor();
    uint16_t count = buf.readUint16();
    std::vector<ScalarKeyframe> result;
    result.reserve(count);
    for (uint16_t i = 0; i < count; ++i) {
        ScalarKeyframe kf;
        kf.frame = buf.readUint16();
        kf.value = buf.readFloat();
        result.push_back(kf);
    }
    return result;
}

// A real CHNK LOCT: float32 averageTranslationSpeed, then uint16 keyCount,
// then `keyCount` real [uint16 frame][float32 x][float32 y][float32 z]
// records (a real, UNCOMPRESSED Vector per keyframe - confirmed directly
// against the leaked original CompressedKeyframeAnimationTemplate.cpp
// source, 2026-07-25, not guessed from byte patterns).
void parseLoct(const IffChunk& chunk, AnimationClipData& result) {
    soe::PacketBuffer buf = chunk.data;
    buf.resetReadCursor();
    result.averageTranslationSpeed = buf.readFloat();
    uint16_t keyCount = buf.readUint16();
    result.locomotionTranslationKeys.reserve(keyCount);
    for (uint16_t i = 0; i < keyCount; ++i) {
        LocomotionKeyframe kf;
        kf.frame = buf.readUint16();
        kf.translation.x = buf.readFloat();
        kf.translation.y = buf.readFloat();
        kf.translation.z = buf.readFloat();
        result.locomotionTranslationKeys.push_back(kf);
    }
}

} // namespace

AnimationClipData AnimationClip::parse(const std::vector<uint8_t>& bytes) {
    auto topLevel = IffReader::parse(bytes);
    if (topLevel.empty() || topLevel[0].id != kFormTag || topLevel[0].formType != kCkatTag) {
        throw std::runtime_error("AnimationClip::parse: not a FORM(CKAT)-rooted file");
    }
    const IffChunk& ckat = topLevel[0];
    if (ckat.children.empty() || ckat.children[0].id != kFormTag) {
        throw std::runtime_error("AnimationClip::parse: missing clip data FORM");
    }
    const IffChunk& clipForm = ckat.children[0];

    const IffChunk* xfrmForm = findFirstForm(clipForm, kXfrmTag);
    const IffChunk* arotForm = findFirstForm(clipForm, kArotTag);
    if (xfrmForm == nullptr || arotForm == nullptr) {
        throw std::runtime_error("AnimationClip::parse: missing XFRM or AROT form");
    }
    const IffChunk* atrnForm = findFirstForm(clipForm, kAtrnTag); // optional - not every real clip animates a translated bone
    const IffChunk* loctChunk = findFirstChunk(clipForm, kLoctTag); // optional - only real locomotion clips (walk/run) have one

    std::vector<const IffChunk*> qchnChunks;
    for (const IffChunk& child : arotForm->children) {
        if (child.id == kQchnTag) {
            qchnChunks.push_back(&child);
        }
    }
    std::vector<const IffChunk*> chnlChunks;
    if (atrnForm != nullptr) {
        for (const IffChunk& child : atrnForm->children) {
            if (child.id == kChnlTag) {
                chnlChunks.push_back(&child);
            }
        }
    }

    AnimationClipData result;
    for (const IffChunk& xfinChunk : xfrmForm->children) {
        if (xfinChunk.id != kXfinTag) {
            continue;
        }
        soe::PacketBuffer buf = xfinChunk.data;
        buf.resetReadCursor();

        AnimationBoneChannel bone;
        bone.boneName = readNulTerminatedString(buf);

        // Real per-bone record (10 bytes, confirmed fixed-size across every
        // real XFIN checked this session, regardless of bone name length):
        // [byte hasRotation][uint16 rotationChannelIndex or a real, separate
        // hardpoint-bone counter when hasRotation==0, not used]
        // [byte hasTranslation][uint16 x3 - real ATRN channel indices when
        // hasTranslation != 0 (confirmed only for `root` this session); a
        // real, separate per-bone counter (likely indexing STRN's unread
        // scale data) when hasTranslation==0].
        uint8_t hasRotation = buf.readByte();
        uint16_t rotationChannelIndex = buf.readUint16();
        uint8_t hasTranslation = buf.readByte();
        uint16_t translationChannelIndex[3];
        translationChannelIndex[0] = buf.readUint16();
        translationChannelIndex[1] = buf.readUint16();
        translationChannelIndex[2] = buf.readUint16();

        // Phase 21 diagnostic (2026-07-24, code audit) - full XFIN record
        // for chain bones, including hasTranslation - previously assumed
        // real translation channels are root-only, never directly checked
        // for this specific chain. A wrongly-applied translation on a
        // mid-chain bone would displace everything below it and compound
        // exactly like the observed symptom.
        if (isFingerChainBone(bone.boneName)) {
            std::printf(
                "[ANIMDBG] XFIN bone=%s hasRotation=%u rotIdx=%u hasTranslation=0x%02x transIdx=(%u,%u,%u)\n",
                bone.boneName.c_str(), hasRotation, rotationChannelIndex, hasTranslation,
                translationChannelIndex[0], translationChannelIndex[1], translationChannelIndex[2]);
        }
        if (hasRotation != 0 && rotationChannelIndex < qchnChunks.size()) {
            bone.rotationKeyframes = parseQchn(*qchnChunks[rotationChannelIndex], bone.boneName);
        }
        // Real bug found live (Phase 21): `hasTranslation` isn't a plain
        // boolean - it's a real per-axis bitmask (bit 3 = X, bit 4 = Y,
        // bit 5 = Z), confirmed via two real samples: 0x38 (0b00111000,
        // bits 3/4/5 all set) on a real clip whose root bone had all 3
        // real translation axes populated, vs 0x10 (0b00010000, bit 4
        // only) on a real walk clip whose root only has a real Y-axis
        // channel (its other two `translationChannelIndex` entries are
        // real but unused padding for the unset axes, not valid CHNL
        // indices - relying on bounds-checking alone to skip them would
        // work by accident in this file but not in general).
        for (int axis = 0; axis < 3; ++axis) {
            bool axisHasTranslation = (hasTranslation & (1u << (3 + axis))) != 0;
            if (!axisHasTranslation) continue;
            uint16_t chnlIdx = translationChannelIndex[axis];
            if (chnlIdx < chnlChunks.size()) {
                bone.translationAxis[axis] = parseChnl(*chnlChunks[chnlIdx]);
            }
        }
        result.bones.push_back(std::move(bone));
    }
    if (loctChunk != nullptr) {
        parseLoct(*loctChunk, result);
    }
    return result;
}

void AnimationClip::setBitLayoutVariantForTesting(int variant) {
    g_bitLayoutVariant = variant;
}

void AnimationClip::setSkipZNegationForArmsForTesting(bool skip) {
    g_experimentSkipZNegationForArms = skip;
}

void AnimationClip::setSkipZNegationForRootForTesting(bool skip) {
    g_experimentSkipZNegationForRoot = skip;
}

void AnimationClip::setSkipZNegationForLegsForTesting(bool skip) {
    g_experimentSkipZNegationForLegs = skip;
}

void AnimationClip::setLegForcedDropIndexForTesting(int forcedIdx) {
    g_legForcedDropIndex = forcedIdx;
}

void AnimationClip::setFingerForcedDropIndexForTesting(int forcedIdx) {
    g_fingerForcedDropIndex = forcedIdx;
}

} // namespace assets
