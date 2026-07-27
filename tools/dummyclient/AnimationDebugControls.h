#pragma once

// Windows/Vulkan only - only ever used by Visualizer.cpp's runVisualizer(),
// itself Windows-only (see Visualizer.h's own top-of-file comment). This
// header is safe to include unconditionally.
#ifdef _WIN32

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "assets/AnimationClip.h"

namespace dummyclient {

// Phase 21 (animation) live-debug aid bundle. Every per-bone composition/
// decode-hypothesis toggle used to isolate this project's various skeletal-
// animation bugs used to live as ~340 lines of scattered `static` locals +
// `renderer::Window::isKeyDown()` blocks directly inside Visualizer.cpp's
// per-frame render loop - moved here so that file stops growing with every
// new debug lever. Kept as PERMANENT debug aids, not stripped once a given
// bug's fixed (several of these predate fixes that are now baked into
// assets::AnimationClip's own decode and stay useful for the NEXT
// hypothesis) - see each field's own comment for why it still matters, and
// PHASE_21_STATUS.md for the full investigation history behind each one.
class AnimationDebugControls {
public:
    // Call once per frame, before reading any of the fields below. Reads
    // every real debug key (B/V/C/T/N/Z/R/X/L/F/G/H/U/M), does the usual
    // edge-detect-and-cycle dance, logs any change to std::cout exactly as
    // before. `clipCache` is cleared directly (not via a flag) whenever a
    // toggle that changes assets::AnimationClip's own static decode
    // behavior flips, matching the original inline behavior exactly - this
    // class doesn't own the cache itself, Visualizer.cpp's runVisualizer()
    // still does.
    void processFrameInput(
        std::unordered_map<std::string, std::optional<assets::AnimationClipData>>& clipCache);

    // Phase 21 (animation) diagnostic - forces self's animated skinning to
    // sample the real BIND pose only (no animation clip), isolating whether
    // a visual bug is in the core skinning math (hierarchy composition,
    // bone-weight binding, inverse-bind-pose) versus the animated-rotation
    // sampling path. 'B' key.
    bool forceBindPoseOnly = false;

    // Phase 21 (animation) diagnostic - cycles which real preRotation/
    // postRotation + animated-rotation composition formula self's skinning
    // uses (see animation::sampleLocalBoneTransforms's own comment on what
    // each variant tests). 'V' key.
    int rotationCompositionVariant = 0;

    // Phase 21 (animation) diagnostic - cycles a real per-axis sign/swap
    // correction applied to the decoded animated quaternion (see
    // animation::sampleLocalBoneTransforms's own comment on
    // `axisFixVariant`). 'C' key.
    int axisFixVariant = 0;

    // Phase 21 (animation) diagnostic - toggles ignoring real animated
    // translation channels entirely (root is the only bone with one). 'T'
    // key.
    bool disableAnimTranslation = false;

    // Phase 21 (animation) diagnostic - toggles which real QCHN bit-layout
    // hypothesis assets::AnimationClip's own decoder uses (see
    // assets::AnimationClip::setBitLayoutVariantForTesting's own comment).
    // 'N' key.
    int bitLayoutVariant = 2;

    // Phase 21 (animation) diagnostic - toggles skipping the Z-negation
    // (see AnimationClip.cpp's own comment) for arm-chain bones only. 'Z'
    // key.
    bool skipZNegationForArms = false;

    // Same idea as skipZNegationForArms but for the `root` bone
    // specifically - root is the one bone whose own decoded rotation
    // directly IS the whole body's world orientation. 'R' key.
    bool skipZNegationForRoot = false;

    // Same idea again, for the leg chain - a direct live user report that
    // legs visibly cross during walking. 'X' key.
    bool skipZNegationForLegs = false;

    // Phase 21 (animation) diagnostic - cycles leg-chain bones through
    // forcing a different fixed dropped axis than the normal "always drop
    // w" rule (off -> x -> y -> z -> w -> off...). 'L' key.
    int legForcedDropIndex = -1;

    // Same idea as legForcedDropIndex but for finger bones
    // (thumb/index/ring). 'F' key.
    int fingerForcedDropIndex = -1;

    // Phase 21 (animation) diagnostic - cycles finger-chain bones (widened
    // to also cover forearm/ulna/wrist - see
    // animation::isFingerChainBoneLocal's own comment) through forcing a
    // different composition variant than every other bone uses. 'G' key.
    int fingerCompositionVariant = -1;

    // Same idea as fingerCompositionVariant but cycles a forced
    // axisFixVariant override for that same chain instead. 'H' key.
    int fingerAxisFixVariant = -1;

    // Phase 21 (animation) diagnostic - toggles the real three-term
    // bind-pose formula found by reading the leaked original client source
    // (see animation::sampleLocalBoneTransforms's own comment on
    // `useRealBindPoseFormula`) - bypasses every rotationCompositionVariant/
    // axisFixVariant/finger-override value above entirely when on. 'U' key.
    bool useRealBindPoseFormula = false;

    // Phase 21 (animation) diagnostic - cumulative bone-isolation chain:
    // the first `isolateBoneCount` bones of the real chain below are the
    // only ones allowed to use their real animated rotation; every other
    // bone is forced to bind pose. Lets a real bone chain be built up one
    // bone at a time to find exactly where multiple simultaneously-animated
    // bones start compounding into a visibly broken result. 0 = normal
    // (every bone animates). 'M' key.
    int isolateBoneCount = 0;
    // Rebuilt every processFrameInput() call from isolateBoneCount and the
    // real chain order (root -> spine -> a leg -> an arm) - pass
    // `&isolateBoneNames` into animation::sampleLocalBoneTransforms exactly
    // as before.
    std::vector<std::string> isolateBoneNames;

private:
    bool bindPoseKeyWasDown_ = false;
    bool variantKeyWasDown_ = false;
    bool axisFixKeyWasDown_ = false;
    bool translationKeyWasDown_ = false;
    bool bitLayoutKeyWasDown_ = false;
    bool armZKeyWasDown_ = false;
    bool rootRKeyWasDown_ = false;
    bool legXKeyWasDown_ = false;
    bool legLKeyWasDown_ = false;
    bool fingerFKeyWasDown_ = false;
    bool fingerGKeyWasDown_ = false;
    bool fingerHKeyWasDown_ = false;
    bool realBindPoseKeyWasDown_ = false;
    bool isolateBoneKeyWasDown_ = false;
};

} // namespace dummyclient

#endif
