#ifdef _WIN32

#include "ScreenshotCapture.h"

#include <DirectXMath.h>

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <system_error>
#include <vector>

#include "PngWriter.h"
#include "renderer/Camera.h"
#include "renderer/VulkanRenderer.h"
#include "renderer/Window.h"

namespace dummyclient {

void ScreenshotCapture::processFrameInput() {
    bool lockedCameraKeyDown = renderer::Window::isKeyDown('K');
    if (lockedCameraKeyDown && !lockedCameraKeyWasDown_) {
        lockedCameraMode = !lockedCameraMode;
        std::cout << "[VISUALIZER] locked diagnostic camera " << (lockedCameraMode ? "ON" : "OFF")
                   << " (angle=" << (lockedCameraAngleIndex * 90) << "deg)\n";
    }
    lockedCameraKeyWasDown_ = lockedCameraKeyDown;

    bool cycleAngleKeyDown = renderer::Window::isKeyDown('J');
    if (cycleAngleKeyDown && !cycleAngleKeyWasDown_ && lockedCameraMode) {
        lockedCameraAngleIndex = (lockedCameraAngleIndex + 1) % 4;
        std::cout << "[VISUALIZER] locked diagnostic camera angle = " << (lockedCameraAngleIndex * 90)
                   << "deg\n";
    }
    cycleAngleKeyWasDown_ = cycleAngleKeyDown;

    bool burstCaptureKeyDown = renderer::Window::isKeyDown('P');
    if (burstCaptureKeyDown && !burstCaptureKeyWasDown_) {
        burstCaptureActive = !burstCaptureActive;
        std::cout << "[VISUALIZER] burst screenshot capture " << (burstCaptureActive ? "STARTED" : "STOPPED")
                   << "\n";
    }
    burstCaptureKeyWasDown_ = burstCaptureKeyDown;
}

void ScreenshotCapture::applyCameraOverride(renderer::FollowCamera& camera,
                                             float selfYawRadians) const {
    if (!lockedCameraMode) {
        return;
    }
    // Set BEFORE camera.update() (not after) so this frame's own position
    // gets computed from the correct forced yaw immediately, rather than
    // lagging a frame behind. Only yaw is forced; update() still applies
    // mouse-driven pitch/zoom normally as long as the right mouse button
    // isn't held (holding it would fight this override, but nothing stops
    // it from being reasserted next frame).
    camera.yaw = selfYawRadians + static_cast<float>(lockedCameraAngleIndex) * (DirectX::XM_PI / 2.0f);
}

void ScreenshotCapture::captureIfActive(renderer::VulkanRenderer& gfx) {
    if (!burstCaptureActive) {
        return;
    }
    auto nowCapture = std::chrono::steady_clock::now();
    if (nowCapture - lastBurstCaptureTime_ <= std::chrono::milliseconds(166)) {
        return;
    }
    lastBurstCaptureTime_ = nowCapture;
    std::vector<uint8_t> pixels;
    uint32_t capturedWidth = 0;
    uint32_t capturedHeight = 0;
    if (!gfx.captureFrameRGBA8(pixels, capturedWidth, capturedHeight)) {
        return;
    }
    std::filesystem::path dir = std::filesystem::path("diagnostic_screenshots") /
                                 (burstCaptureFolderOverride.empty()
                                      ? (std::to_string(lockedCameraAngleIndex * 90) + "deg")
                                      : burstCaptureFolderOverride);
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    char frameFile[64];
    std::snprintf(frameFile, sizeof(frameFile), "frame_%04d.png", burstCaptureFrameCounter++);
    std::string outPath = (dir / frameFile).string();
    if (dummyclient::writePngRGBA8(outPath, capturedWidth, capturedHeight, pixels)) {
        std::cout << "[VISUALIZER] captured " << outPath << "\n";
    } else {
        std::cout << "[VISUALIZER] failed to write " << outPath << "\n";
    }
}

} // namespace dummyclient

#endif
