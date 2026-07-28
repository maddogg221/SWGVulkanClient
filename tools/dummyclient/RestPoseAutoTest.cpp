#ifdef _WIN32

#include "RestPoseAutoTest.h"

#include <iostream>

namespace dummyclient {

RestPoseAutoTest::RestPoseAutoTest(int formulaSecondsPerPhase, int variantSweepSecondsPerPhase,
                                    int bindAxisSweepSecondsPerPhase)
    : formulaSecondsPerPhase_(formulaSecondsPerPhase),
      variantSweepSecondsPerPhase_(variantSweepSecondsPerPhase),
      bindAxisSweepSecondsPerPhase_(bindAxisSweepSecondsPerPhase),
      startTime_(std::chrono::steady_clock::now()) {}

void RestPoseAutoTest::driveFrame(AnimationDebugControls& controls, ScreenshotCapture& capture) {
    float elapsed = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime_).count();

    if (bindAxisSweepSecondsPerPhase_ > 0) {
        float phaseSeconds = static_cast<float>(bindAxisSweepSecondsPerPhase_);
        int phase = static_cast<int>(elapsed / phaseSeconds);
        if (phase == lastPhaseLogged_) {
            return;
        }
        lastPhaseLogged_ = phase;
        if (phase >= 7) {
            capture.burstCaptureActive = false;
            complete_ = true;
            std::cout << "[VISUALIZER] auto rest-pose bind-axis sweep complete, exiting\n";
            return;
        }
        controls.bindRotationAxisFixVariant = phase;
        capture.burstCaptureFolderOverride = "restpose_bindaxis_" + std::to_string(phase);
        capture.burstCaptureActive = true;
        capture.lockedCameraMode = true;
        capture.lockedCameraAngleIndex = 0;
        std::cout << "[VISUALIZER] auto rest-pose bind-axis sweep phase " << phase
                   << " (bindRotationAxisFixVariant=" << controls.bindRotationAxisFixVariant
                   << ") - capturing to diagnostic_screenshots/" << capture.burstCaptureFolderOverride
                   << "/\n";
        return;
    }

    if (variantSweepSecondsPerPhase_ > 0) {
        float phaseSeconds = static_cast<float>(variantSweepSecondsPerPhase_);
        int phase = static_cast<int>(elapsed / phaseSeconds);
        if (phase == lastPhaseLogged_) {
            return;
        }
        lastPhaseLogged_ = phase;
        if (phase >= 6) {
            capture.burstCaptureActive = false;
            complete_ = true;
            std::cout << "[VISUALIZER] auto rest-pose variant sweep complete, exiting\n";
            return;
        }
        controls.rotationCompositionVariant = phase;
        capture.burstCaptureFolderOverride = "restpose_variant_" + std::to_string(phase);
        capture.burstCaptureActive = true;
        capture.lockedCameraMode = true;
        capture.lockedCameraAngleIndex = 0;
        std::cout << "[VISUALIZER] auto rest-pose variant sweep phase " << phase
                   << " (rotationCompositionVariant=" << controls.rotationCompositionVariant
                   << ") - capturing to diagnostic_screenshots/" << capture.burstCaptureFolderOverride
                   << "/\n";
        return;
    }

    if (formulaSecondsPerPhase_ > 0) {
        float phaseSeconds = static_cast<float>(formulaSecondsPerPhase_);
        int phase = elapsed < phaseSeconds ? 0 : (elapsed < 2.0f * phaseSeconds ? 1 : 2);
        if (phase == lastPhaseLogged_) {
            return;
        }
        lastPhaseLogged_ = phase;
        if (phase == 2) {
            capture.burstCaptureActive = false;
            complete_ = true;
            std::cout << "[VISUALIZER] auto rest-pose test complete, exiting\n";
            return;
        }
        controls.useRealBindPoseFormula = (phase == 1);
        capture.burstCaptureFolderOverride = phase == 0 ? "restpose_formula_off" : "restpose_formula_on";
        capture.burstCaptureActive = true;
        capture.lockedCameraMode = true;
        capture.lockedCameraAngleIndex = 0;
        std::cout << "[VISUALIZER] auto rest-pose test phase " << phase << " (bind-pose formula "
                   << (controls.useRealBindPoseFormula ? "REAL/3-term" : "OLD/2-term")
                   << ") - capturing to diagnostic_screenshots/" << capture.burstCaptureFolderOverride
                   << "/\n";
    }
}

} // namespace dummyclient

#endif
