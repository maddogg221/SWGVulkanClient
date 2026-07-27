#pragma once

// Windows/Vulkan only - only ever used by Visualizer.cpp's runVisualizer(),
// itself Windows-only (see Visualizer.h's own top-of-file comment). This
// header is safe to include unconditionally.
#ifdef _WIN32

#include <chrono>

#include "AnimationDebugControls.h"
#include "ScreenshotCapture.h"

namespace dummyclient {

// Phase 21 rest-pose investigation - a headless, keyboard-free diagnostic
// driver, added after an earlier attempt at scripting this via simulated
// keyboard/window-focus input (SetForegroundWindow/SendKeys) grabbed focus
// on the WRONG window entirely - real per-frame state changes triggered
// directly through the same AnimationDebugControls/ScreenshotCapture state
// the 'K'/'U'/'V'/'P' keys already drive, no OS input injection involved.
//
// Two mutually-exclusive modes (only one CLI flag is ever passed at a
// time), both captured to a locked camera angle:
//   - formulaSecondsPerPhase > 0: A/B tests the old two-term vs. real
//     three-term bind-pose formula (see AnimationDebugControls::
//     useRealBindPoseFormula), capturing to diagnostic_screenshots/
//     restpose_formula_off/ then restpose_formula_on/.
//   - variantSweepSecondsPerPhase > 0: sweeps all 6
//     rotationCompositionVariant values, capturing to
//     diagnostic_screenshots/restpose_variant_<N>/.
class RestPoseAutoTest {
public:
    RestPoseAutoTest(int formulaSecondsPerPhase, int variantSweepSecondsPerPhase);

    // False (the common case) means neither CLI flag was passed - drive()
    // should never be called, and complete() will always be false, so the
    // caller's own `!complete()` loop condition never forces an exit.
    bool enabled() const { return formulaSecondsPerPhase_ > 0 || variantSweepSecondsPerPhase_ > 0; }

    // Once true, the requested auto-test has finished every phase - the
    // caller (runVisualizer's main loop) should end the render loop.
    // Always false when enabled() is false.
    bool complete() const { return complete_; }

    // Call once per frame when enabled(). Overrides `controls`/`capture`
    // state exactly as the manual key toggles would, just driven by
    // elapsed real time (since construction) instead of keyboard input.
    void driveFrame(AnimationDebugControls& controls, ScreenshotCapture& capture);

private:
    int formulaSecondsPerPhase_;
    int variantSweepSecondsPerPhase_;
    std::chrono::steady_clock::time_point startTime_;
    bool complete_ = false;
    int lastPhaseLogged_ = -1;
};

} // namespace dummyclient

#endif
