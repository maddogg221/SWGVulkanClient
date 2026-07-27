#pragma once

// Windows/Vulkan only - only ever used by Visualizer.cpp's runVisualizer(),
// itself Windows-only (see Visualizer.h's own top-of-file comment). This
// header is safe to include unconditionally.
#ifdef _WIN32

#include <chrono>
#include <cstdint>
#include <string>

namespace renderer {
struct FollowCamera;
class VulkanRenderer;
}

namespace dummyclient {

// Live visual-comparison diagnostic aid, added for walk-animation debugging
// - describing a moving defect in words or a single screenshot kept missing
// the actual problem. Locks the normal FollowCamera's yaw to one of 4 fixed
// offsets from self's own current facing (front/right/back/left, matched to
// what the real official client can be manually aligned to via its own
// compass/heading readout - see PHASE_21_STATUS.md) instead of the usual
// mouse-driven orbit, then optionally captures a burst of real PNG frames at
// a fixed interval to a per-angle folder so a full walk cycle (or any other
// live comparison) can be reviewed frame-by-frame afterward, not just at
// whatever moment a manual screenshot happened to land on. Pitch/zoom
// (mouse wheel) still work normally while locked - only yaw is forced.
//
// Moved out of Visualizer.cpp's own per-frame loop, where this used to be
// ~90 lines of scattered state + 'K'/'J'/'P' isKeyDown() blocks + an
// end-of-frame PNG-write block.
class ScreenshotCapture {
public:
    // Call once per frame. Reads 'K' (toggle locked-camera mode), 'J'
    // (cycle which of the 4 fixed angles is active, only meaningful while
    // locked), and 'P' (start/stop a burst capture at whatever angle is
    // currently locked) - same edge-detect-and-log behavior as before.
    void processFrameInput();

    // Call every frame BEFORE camera.update(), only when not in the
    // separate full-building inspection mode (that uses its own free
    // FlyCamera, unaffected by this). No-op unless locked-camera mode is
    // currently on. Forces camera.yaw from selfYawRadians + whichever of
    // the 4 fixed angles is active - see lockedCameraMode's own history for
    // why this must happen BEFORE update(), not after.
    void applyCameraOverride(renderer::FollowCamera& camera, float selfYawRadians) const;

    // Call once per frame, right after gfx.endFrame() and before the next
    // beginFrame() (see renderer::VulkanRenderer::captureFrameRGBA8's own
    // comment on why). No-op unless a burst capture is currently active;
    // throttles itself to ~6 real frames/sec so a walk cycle produces a
    // reviewable number of frames instead of thousands. Writes into
    // diagnostic_screenshots/<folderOverride, defaulting to the current
    // locked angle in degrees>/frame_NNNN.png.
    void captureIfActive(renderer::VulkanRenderer& gfx);

    // True while locked-camera mode is on (read by Visualizer.cpp to decide
    // whether applyCameraOverride() is meaningful this frame - inspection
    // mode branches around both).
    bool lockedCameraMode = false;
    int lockedCameraAngleIndex = 0; // 0/1/2/3 -> 0/90/180/270 degrees

    bool burstCaptureActive = false;
    int burstCaptureFrameCounter = 0;

    // Set by RestPoseAutoTest (or any future automated capture driver) to
    // route capture output into a named folder instead of the usual
    // per-angle one - empty means "use the normal <angle>deg folder".
    std::string burstCaptureFolderOverride;

private:
    bool lockedCameraKeyWasDown_ = false;
    bool cycleAngleKeyWasDown_ = false;
    bool burstCaptureKeyWasDown_ = false;
    std::chrono::steady_clock::time_point lastBurstCaptureTime_{};
};

} // namespace dummyclient

#endif
