#ifdef _WIN32

#include "AnimationDebugControls.h"

#include <iostream>

#include "renderer/Window.h"

namespace dummyclient {

namespace {

// Phase 21 (animation) diagnostic - real bone chain (root -> spine -> a leg
// -> an arm) used by the cumulative bone-isolation cycle ('M' key) - see
// isolateBoneCount's own header comment.
constexpr const char* kIsolateBoneChain[] = {
    "root",   "spine1", "spine2", "spine3", "neck",  "head",
    "lThigh", "lShin",  "lAnkle", "ltoe",   "lClav", "lArm",
    "lForeArm", "lUlna", "lWrist",
};
constexpr int kIsolateBoneChainLen =
    static_cast<int>(sizeof(kIsolateBoneChain) / sizeof(kIsolateBoneChain[0]));

} // namespace

void AnimationDebugControls::processFrameInput(
    std::unordered_map<std::string, std::optional<assets::AnimationClipData>>& clipCache) {
    bool bindPoseKeyDown = renderer::Window::isKeyDown('B');
    if (bindPoseKeyDown && !bindPoseKeyWasDown_) {
        forceBindPoseOnly = !forceBindPoseOnly;
        std::cout << "[VISUALIZER] force bind-pose-only self skinning "
                   << (forceBindPoseOnly ? "ON" : "OFF") << "\n";
    }
    bindPoseKeyWasDown_ = bindPoseKeyDown;

    bool variantKeyDown = renderer::Window::isKeyDown('V');
    if (variantKeyDown && !variantKeyWasDown_) {
        rotationCompositionVariant = (rotationCompositionVariant + 1) % 6;
        std::cout << "[VISUALIZER] self rotation composition variant = " << rotationCompositionVariant
                   << "\n";
    }
    variantKeyWasDown_ = variantKeyDown;

    bool axisFixKeyDown = renderer::Window::isKeyDown('C');
    if (axisFixKeyDown && !axisFixKeyWasDown_) {
        axisFixVariant = (axisFixVariant + 1) % 7;
        std::cout << "[VISUALIZER] self axis-fix variant = " << axisFixVariant << "\n";
    }
    axisFixKeyWasDown_ = axisFixKeyDown;

    bool translationKeyDown = renderer::Window::isKeyDown('T');
    if (translationKeyDown && !translationKeyWasDown_) {
        disableAnimTranslation = !disableAnimTranslation;
        std::cout << "[VISUALIZER] disable self animated translation "
                   << (disableAnimTranslation ? "ON" : "OFF") << "\n";
    }
    translationKeyWasDown_ = translationKeyDown;

    bool bitLayoutKeyDown = renderer::Window::isKeyDown('N');
    if (bitLayoutKeyDown && !bitLayoutKeyWasDown_) {
        bitLayoutVariant = (bitLayoutVariant + 1) % 3;
        assets::AnimationClip::setBitLayoutVariantForTesting(bitLayoutVariant);
        clipCache.clear();
        std::cout << "[VISUALIZER] self QCHN bit-layout variant = " << bitLayoutVariant
                   << " (clip cache cleared)\n";
    }
    bitLayoutKeyWasDown_ = bitLayoutKeyDown;

    bool armZKeyDown = renderer::Window::isKeyDown('Z');
    if (armZKeyDown && !armZKeyWasDown_) {
        skipZNegationForArms = !skipZNegationForArms;
        assets::AnimationClip::setSkipZNegationForArmsForTesting(skipZNegationForArms);
        clipCache.clear();
        std::cout << "[VISUALIZER] self skip-Z-negation-for-arms = "
                   << (skipZNegationForArms ? "ON" : "OFF") << " (clip cache cleared)\n";
    }
    armZKeyWasDown_ = armZKeyDown;

    bool rootRKeyDown = renderer::Window::isKeyDown('R');
    if (rootRKeyDown && !rootRKeyWasDown_) {
        skipZNegationForRoot = !skipZNegationForRoot;
        assets::AnimationClip::setSkipZNegationForRootForTesting(skipZNegationForRoot);
        clipCache.clear();
        std::cout << "[VISUALIZER] self skip-Z-negation-for-root = "
                   << (skipZNegationForRoot ? "ON" : "OFF") << " (clip cache cleared)\n";
    }
    rootRKeyWasDown_ = rootRKeyDown;

    bool legXKeyDown = renderer::Window::isKeyDown('X');
    if (legXKeyDown && !legXKeyWasDown_) {
        skipZNegationForLegs = !skipZNegationForLegs;
        assets::AnimationClip::setSkipZNegationForLegsForTesting(skipZNegationForLegs);
        clipCache.clear();
        std::cout << "[VISUALIZER] self skip-Z-negation-for-legs = "
                   << (skipZNegationForLegs ? "ON" : "OFF") << " (clip cache cleared)\n";
    }
    legXKeyWasDown_ = legXKeyDown;

    bool legLKeyDown = renderer::Window::isKeyDown('L');
    if (legLKeyDown && !legLKeyWasDown_) {
        legForcedDropIndex = legForcedDropIndex + 1 >= 4 ? -1 : legForcedDropIndex + 1;
        assets::AnimationClip::setLegForcedDropIndexForTesting(legForcedDropIndex);
        clipCache.clear();
        std::cout << "[VISUALIZER] self leg-forced-drop-index = " << legForcedDropIndex
                   << " (-1=off/normal always-w, clip cache cleared)\n";
    }
    legLKeyWasDown_ = legLKeyDown;

    bool fingerFKeyDown = renderer::Window::isKeyDown('F');
    if (fingerFKeyDown && !fingerFKeyWasDown_) {
        fingerForcedDropIndex = fingerForcedDropIndex + 1 >= 4 ? -1 : fingerForcedDropIndex + 1;
        assets::AnimationClip::setFingerForcedDropIndexForTesting(fingerForcedDropIndex);
        clipCache.clear();
        std::cout << "[VISUALIZER] self finger-forced-drop-index = " << fingerForcedDropIndex
                   << " (-1=off/normal always-w, clip cache cleared)\n";
    }
    fingerFKeyWasDown_ = fingerFKeyDown;

    bool fingerGKeyDown = renderer::Window::isKeyDown('G');
    if (fingerGKeyDown && !fingerGKeyWasDown_) {
        fingerCompositionVariant = fingerCompositionVariant + 1 >= 6 ? -1 : fingerCompositionVariant + 1;
        clipCache.clear();
        std::cout << "[VISUALIZER] self finger-composition-variant = " << fingerCompositionVariant
                   << " (-1=off/normal, clip cache cleared)\n";
    }
    fingerGKeyWasDown_ = fingerGKeyDown;

    bool fingerHKeyDown = renderer::Window::isKeyDown('H');
    if (fingerHKeyDown && !fingerHKeyWasDown_) {
        fingerAxisFixVariant = fingerAxisFixVariant + 1 >= 7 ? -1 : fingerAxisFixVariant + 1;
        clipCache.clear();
        std::cout << "[VISUALIZER] self finger-axis-fix-variant = " << fingerAxisFixVariant
                   << " (-1=off/normal, clip cache cleared)\n";
    }
    fingerHKeyWasDown_ = fingerHKeyDown;

    bool realBindPoseKeyDown = renderer::Window::isKeyDown('U');
    if (realBindPoseKeyDown && !realBindPoseKeyWasDown_) {
        useRealBindPoseFormula = !useRealBindPoseFormula;
        std::cout << "[VISUALIZER] self use-real-bind-pose-formula = "
                   << (useRealBindPoseFormula ? "ON" : "OFF") << "\n";
    }
    realBindPoseKeyWasDown_ = realBindPoseKeyDown;

    bool isolateBoneKeyDown = renderer::Window::isKeyDown('M');
    if (isolateBoneKeyDown && !isolateBoneKeyWasDown_) {
        isolateBoneCount = (isolateBoneCount + 1) % (kIsolateBoneChainLen + 1);
        if (isolateBoneCount == 0) {
            std::cout << "[VISUALIZER] self isolate-bones = (none, all bones animate)\n";
        } else {
            std::cout << "[VISUALIZER] self isolate-bones = ";
            for (int bi = 0; bi < isolateBoneCount; ++bi) {
                std::cout << kIsolateBoneChain[bi] << (bi + 1 < isolateBoneCount ? "+" : "");
            }
            std::cout << "\n";
        }
    }
    isolateBoneKeyWasDown_ = isolateBoneKeyDown;
    isolateBoneNames.assign(kIsolateBoneChain, kIsolateBoneChain + isolateBoneCount);
}

} // namespace dummyclient

#endif
