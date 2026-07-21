#include "renderer/Camera.h"

#include <algorithm>

#include <Windows.h>

#include "renderer/Window.h"

using namespace DirectX;

namespace renderer {

namespace {
constexpr float kPitchLimit = XM_PIDIV2 - 0.01f; // just short of straight up/down
}

void FlyCamera::update(Window& window, float deltaSeconds) {
    if (window.isMouseButtonDown(1)) { // right mouse button held: look around
        int dx = 0;
        int dy = 0;
        window.takeMouseDelta(dx, dy);
        yaw += static_cast<float>(dx) * mouseSensitivity;
        pitch -= static_cast<float>(dy) * mouseSensitivity; // screen Y grows downward
        pitch = std::clamp(pitch, -kPitchLimit, kPitchLimit);
    } else {
        // Still drain the accumulator even while not looking around, so a
        // burst of movement while the button was released doesn't cause a
        // sudden jump the next time it's held.
        int dx = 0;
        int dy = 0;
        window.takeMouseDelta(dx, dy);
    }

    XMFLOAT3 fwd = forward();
    XMVECTOR forwardVec = XMLoadFloat3(&fwd);
    XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR rightVec = XMVector3Normalize(XMVector3Cross(worldUp, forwardVec));

    XMVECTOR posVec = XMLoadFloat3(&position);
    float distance = moveSpeed * deltaSeconds;

    if (Window::isKeyDown('W')) posVec = XMVectorAdd(posVec, XMVectorScale(forwardVec, distance));
    if (Window::isKeyDown('S')) posVec = XMVectorSubtract(posVec, XMVectorScale(forwardVec, distance));
    if (Window::isKeyDown('A')) posVec = XMVectorSubtract(posVec, XMVectorScale(rightVec, distance));
    if (Window::isKeyDown('D')) posVec = XMVectorAdd(posVec, XMVectorScale(rightVec, distance));
    if (Window::isKeyDown(VK_SPACE)) posVec = XMVectorAdd(posVec, XMVectorScale(worldUp, distance));
    if (Window::isKeyDown(VK_CONTROL))
        posVec = XMVectorSubtract(posVec, XMVectorScale(worldUp, distance));

    XMStoreFloat3(&position, posVec);
}

XMFLOAT3 FlyCamera::forward() const {
    // Standard yaw/pitch -> direction vector (right-handed, Y-up): yaw
    // rotates around Y starting from +Z, pitch tilts up/down afterward.
    float cosPitch = cosf(pitch);
    XMFLOAT3 dir{sinf(yaw) * cosPitch, sinf(pitch), cosf(yaw) * cosPitch};
    return dir;
}

XMMATRIX FlyCamera::viewMatrix() const {
    XMVECTOR posVec = XMLoadFloat3(&position);
    XMFLOAT3 fwd = forward();
    XMVECTOR forwardVec = XMLoadFloat3(&fwd);
    XMVECTOR targetVec = XMVectorAdd(posVec, forwardVec);
    XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return XMMatrixLookAtLH(posVec, targetVec, upVec);
}

void FollowCamera::update(Window& window, const XMFLOAT3& targetPosition, float deltaSeconds) {
    (void)deltaSeconds; // no smoothing/lag on this first pass

    if (window.isMouseButtonDown(1)) { // right mouse button held: orbit
        int dx = 0;
        int dy = 0;
        window.takeMouseDelta(dx, dy);
        yaw += static_cast<float>(dx) * mouseSensitivity;
        pitch -= static_cast<float>(dy) * mouseSensitivity;
        pitch = std::clamp(pitch, -kPitchLimit, kPitchLimit);
    } else {
        // Drain the accumulator even while not orbiting, matching
        // FlyCamera::update()'s own reasoning - avoids a jump the next time
        // the button is held after a burst of unrelated mouse movement.
        int dx = 0;
        int dy = 0;
        window.takeMouseDelta(dx, dy);
    }

    int wheel = window.takeMouseWheelDelta();
    if (wheel != 0) {
        distance -= (static_cast<float>(wheel) / WHEEL_DELTA) * zoomPerWheelNotch;
        distance = std::clamp(distance, minDistance, maxDistance);
    }

    XMFLOAT3 dir = lookDirection();
    XMVECTOR dirVec = XMLoadFloat3(&dir);
    XMVECTOR targetVec =
        XMVectorSet(targetPosition.x, targetPosition.y + targetHeightOffset, targetPosition.z, 0.0f);
    XMVECTOR posVec = XMVectorSubtract(targetVec, XMVectorScale(dirVec, distance));
    XMStoreFloat3(&position, posVec);
}

XMFLOAT3 FollowCamera::lookDirection() const {
    // Same yaw/pitch -> direction convention as FlyCamera::forward().
    float cosPitch = cosf(pitch);
    return XMFLOAT3{sinf(yaw) * cosPitch, sinf(pitch), cosf(yaw) * cosPitch};
}

XMMATRIX FollowCamera::viewMatrix(const XMFLOAT3& targetPosition) const {
    XMVECTOR eyeVec = XMLoadFloat3(&position);
    XMVECTOR targetVec =
        XMVectorSet(targetPosition.x, targetPosition.y + targetHeightOffset, targetPosition.z, 0.0f);
    XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    return XMMatrixLookAtLH(eyeVec, targetVec, upVec);
}

} // namespace renderer
