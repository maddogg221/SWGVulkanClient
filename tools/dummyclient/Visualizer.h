#pragma once

// Windows/Vulkan only - see Visualizer.cpp's own top-of-file comment. This
// header is safe to include unconditionally; runVisualizer() itself only
// exists when _WIN32 is defined, matching main.cpp's existing #ifdef guard
// around its one call site.
#ifdef _WIN32

#include <string>

namespace soe {
class SoeSession;
class MessageDispatcher;
}
namespace swgproto {
class ObjControllerDispatcher;
}
namespace worldmodel {
class ObjectStore;
}

// The crude wireframe/real-mesh visualizer (Windows/Vulkan only - see
// libs/renderer). Extracted out of main.cpp 2026-07-18 (was ~640 lines
// embedded in a 2200+-line file) - this is application-level glue specific
// to dummyclient, not general-purpose enough for libs/renderer or
// libs/assets themselves, so it stays in tools/dummyclient rather than
// becoming a new library.
// `autoRestPoseTestSecondsPerPhase` (0 = disabled, the default): a headless,
// keyboard-free diagnostic driver for the Phase 21 rest-pose investigation.
// Deliberately avoids driving this via simulated keyboard/window-focus input
// (an earlier attempt at that grabbed focus on the WRONG window entirely -
// SetForegroundWindow silently resolved to something other than this
// process's own render window) - real per-frame state changes triggered
// directly by this same code path the 'K'/'U'/'P' keys already use, no OS
// input injection involved. When > 0: locks the camera to angle 0 relative
// to self, captures a burst with the OLD two-term (preRot*postRot) bind-pose
// formula for this many real seconds into diagnostic_screenshots/
// restpose_formula_off/, then switches to the real three-term formula (see
// SkeletalPose.h's own comment on `useRealBindPoseFormula`) for the same
// duration into diagnostic_screenshots/restpose_formula_on/, then exits the
// render loop cleanly on its own.
// `autoRestPoseVariantSweepSecondsPerPhase` (0 = disabled): same idea and
// same safety rationale as `autoRestPoseTestSecondsPerPhase` above, but
// sweeps the existing `rotationCompositionVariant` debug lever (normally
// the `'V'` key, see SkeletalPose.h's own comment on what each of the 6
// variants tests) through all 6 values instead of the bind-pose-formula
// toggle - real per-bone rotation composition ORDER, which the bind-pose-
// formula test above never actually isolated (that test's premise -
// `animRot` is identity at rest - turned out to be wrong: `loop_standing`
// resolves to a real, active idle-breathing clip, not literal bind pose).
// Captures each variant's burst into diagnostic_screenshots/
// restpose_variant_<N>/, `secondsPerPhase` real seconds each, then exits.
void runVisualizer(soe::SoeSession& zoneSession, soe::MessageDispatcher& dispatcher, bool& failed,
                    swgproto::ObjControllerDispatcher& objControllerDispatcher,
                    const worldmodel::ObjectStore& objectStore, const std::string& clientPath,
                    const std::string& terrainName, int autoRestPoseTestSecondsPerPhase = 0,
                    int autoRestPoseVariantSweepSecondsPerPhase = 0);

#endif
