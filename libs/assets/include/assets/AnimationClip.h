#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/Skeleton.h" // Quaternion

namespace assets {

struct QuaternionKeyframe {
    uint16_t frame = 0;
    Quaternion rotation;
};

struct ScalarKeyframe {
    uint16_t frame = 0;
    float value = 0.0f;
};

// One real animated bone's channel data from a `.ans` clip, keyed by bone
// NAME (not index - see AnimationClip.h's own header comment on why; a
// clip's bone list isn't guaranteed to share index order with either the
// `.skt` skeleton or the `.mgn` mesh's own bone lists). A bone has EITHER a
// rotation channel (the common case - most bones just rotate relative to
// their parent) OR up to 3 independent per-axis translation channels
// (confirmed real for `root` only, in every sample checked this session) -
// never both, in the real files checked. `translationAxis[i]` (i=0,1,2 for
// x,y,z) are three SEPARATE real per-axis scalar curves - the real file's
// per-axis keyframe counts differed in the sample checked (27/28/28), so
// they do NOT necessarily share keyframe times; sample each axis
// independently and combine into a position at playback time. A bone with
// neither (a real "hardpoint" attachment bone, e.g. `sword`/`hold_r`) has
// both empty and should just follow its parent's bind-pose-relative
// transform unmodified.
struct AnimationBoneChannel {
    std::string boneName;
    std::vector<QuaternionKeyframe> rotationKeyframes;
    std::vector<ScalarKeyframe> translationAxis[3];
};

// One real locomotion-curve keyframe from a real CHNK LOCT (see
// AnimationClipData's own comment) - a real, UNCOMPRESSED translation
// (unlike per-bone ATRN channels, which store x/y/z as three separate
// scalar curves, LOCT stores one real Vector per keyframe directly).
struct LocomotionKeyframe {
    uint16_t frame = 0;
    Float3 translation;
};

// Plain data from a real ".ans" file (FORM CKAT) - a per-bone keyframe
// animation clip. Real structure confirmed by dumping several actual
// extracted .ans files (see AnimationClip.cpp for the exact byte layout of
// each chunk): FORM(CKAT) -> FORM(0001) -> CHNK INFO (16 bytes, header
// fields incl. frame rate - not decoded/needed this phase) + FORM(XFRM)
// (real per-bone CHNK XFIN records, name + channel-index data) +
// FORM(AROT) (real per-bone-i-animated-bone CHNK QCHN rotation-keyframe
// channels) + CHNK SROT (49 bytes, likely default/rest rotations for
// unanimated bones - not decoded) + FORM(ATRN) (real per-axis CHNK CHNL
// translation-keyframe channels, only ever 3 total in samples checked -
// one clip only ever animates one bone's translation, `root`) +
// CHNK STRN (real per-bone scale-channel floats, confirmed all-zero/unused
// in every sample checked - not decoded) + CHNK LOCT (real locomotion
// data, confirmed 2026-07-25 directly against the leaked original
// CompressedKeyframeAnimationTemplate.cpp source - a real average
// translation speed float, plus a real, separate, UNCOMPRESSED translation
// keyframe curve, distinct from any per-bone channel above and distinct
// from `root`'s own ATRN channel - the real client uses this specifically
// to SCALE animation playback speed to match a character's actual real
// movement speed (`getScaledLocomotion`), which this project's own
// playback previously never did, always advancing at a fixed rate
// regardless of real movement speed).
struct AnimationClipData {
    std::vector<AnimationBoneChannel> bones;
    // 0.0f if this clip has no real LOCT chunk at all (a real, valid case
    // - non-locomotion clips, e.g. idle/combat-move clips, don't have one).
    float averageTranslationSpeed = 0.0f;
    std::vector<LocomotionKeyframe> locomotionTranslationKeys;
};

class AnimationClip {
public:
    // Throws std::runtime_error if the buffer doesn't parse as expected.
    static AnimationClipData parse(const std::vector<uint8_t>& bytes);

    // Phase 21 live-debug aid - selects which real bit-layout hypothesis
    // decodeSmallestThreeQuaternion() (AnimationClip.cpp) uses to decode a
    // real QCHN keyframe's quantized rotation payload:
    //   0 = idx in the top 2 bits, unsigned-centered fields (original guess)
    //   1 = idx in the bottom 2 bits, signed two's-complement fields (original guess)
    //   2 (default) = the CONFIRMED real layout, live x32dbg RE'd from the
    //     official client 2026-07-22 morning (traced the real file loader
    //     from the "CKAT"/"INFO"/"XFRM"/"XFIN"/"AROT"/"QCHN" tag reads down
    //     to the real per-field decode math): asymmetric 11/11/10-bit
    //     SIGNED-MAGNITUDE fields (each field's own top bit is an explicit
    //     sign flag, not two's complement), dropped component always the
    //     4th (w) - confirmed there is no stored per-keyframe OR
    //     per-channel "which axis was dropped" index anywhere; the 3-byte
    //     per-channel value this project reads and discards is confirmed
    //     to be 3 SEPARATE per-component scale-table-row-selector bytes,
    //     not an index. Variant 2's scale/base per component is still only
    //     an APPROXIMATION (symmetric ±kScale, matching variants 0/1's own
    //     convention) since the real scale-table contents at
    //     swgemu.exe:018AF650/018AF658 were not extracted - only the bit
    //     boundaries, sign handling, and fixed w-drop are confirmed exact.
    // Affects every subsequent parse() call - callers changing this must
    // re-parse/re-cache any already-parsed clips. Not thread-safe; fine for
    // this project's single-threaded self-only v1 animation path.
    static void setBitLayoutVariantForTesting(int variant);

    // Phase 21 live-debug aid - when true, skips the Z-negation applied to
    // every decoded rotation (see AnimationClip.cpp's own comment on it) for
    // arm-chain bones specifically (larm/lforearm/lulna/lwrist and their r*
    // counterparts), leaving every other bone unaffected. The Z-negation was
    // found and validated using leg motion only; arms have been the most
    // persistently malformed bone group in live testing, so this tests
    // whether arms need a different real axis convention than legs/spine.
    // Affects every subsequent parse() call. Not thread-safe; fine for this
    // project's single-threaded self-only v1 animation path.
    static void setSkipZNegationForArmsForTesting(bool skip);

    // Phase 21 live-debug aid - same idea as setSkipZNegationForArmsForTesting
    // but for the `root` bone specifically. Root is architecturally unique
    // (no parent), so its own decoded rotation directly IS the whole body's
    // world orientation - live evidence (self flipping/twisting while
    // walking, but stable in bind pose) isolates the remaining problem to
    // the walk clip's decoded rotation data, and root is the one bone whose
    // error would visibly rotate the entire body. Affects every subsequent
    // parse() call. Not thread-safe; fine for this project's
    // single-threaded self-only v1 animation path.
    static void setSkipZNegationForRootForTesting(bool skip);

    // Phase 21 live-debug aid, added 2026-07-25 - same idea as
    // setSkipZNegationForArmsForTesting but for the leg chain
    // (lthigh/lshin/lankle/ltoe + r* counterparts). Direct user report from
    // a real live walk cycle: the legs visibly CROSS during walking - a
    // real, distinctive signature of a lateral/handedness sign error, the
    // same class of bug already confirmed and fixed for the wrist/finger
    // chain this session. Affects every subsequent parse() call. Not
    // thread-safe; fine for this project's single-threaded self-only v1
    // animation path.
    static void setSkipZNegationForLegsForTesting(bool skip);

    // Phase 21 live-debug aid - bit-layout variant 2's "always drop w" rule
    // (see setBitLayoutVariantForTesting's own comment) fixed the
    // whole-body orientation and most limbs live, but legs specifically
    // still show real artifacts (flat "sail" triangles, twisting) even
    // with that fix - suggesting legs may genuinely drop a different fixed
    // axis than w, since no per-keyframe/per-channel selector was found
    // stored anywhere in the real client. -1 (default) = disabled, legs
    // use the normal always-w rule same as every other bone; 0/1/2/3 =
    // force leg-chain bones (lthigh/lshin/lankle/ltoe and r* counterparts)
    // specifically to drop that axis instead, to empirically find the real
    // one. Affects every subsequent parse() call. Not thread-safe; fine
    // for this project's single-threaded self-only v1 animation path.
    static void setLegForcedDropIndexForTesting(int forcedIdx);

    // Phase 21 live-debug aid - same idea as setLegForcedDropIndexForTesting
    // but for finger bones (thumb/index/ring). Live evidence: extending the
    // arm-chain Z-negation fix to fingers (same treatment that fixed arms)
    // gave a NEGATIVE result - it flipped the hand artifact's direction
    // rather than fixing it, ruling out a simple sign issue but not a
    // different fixed dropped axis. -1 (default) = disabled, fingers use
    // the normal always-w rule; 0/1/2/3 = force finger-chain bones
    // specifically to drop that axis instead. Affects every subsequent
    // parse() call. Not thread-safe; fine for this project's
    // single-threaded self-only v1 animation path.
    static void setFingerForcedDropIndexForTesting(int forcedIdx);
};

} // namespace assets
