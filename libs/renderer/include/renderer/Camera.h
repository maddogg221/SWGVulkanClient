#pragma once

#include <array>

#include <DirectXMath.h>

namespace renderer {

class Window;

// A simple fly camera: WASD to move (camera-relative forward/strafe), Space/
// Ctrl to move straight up/down (world-relative, not camera-relative - keeps
// vertical movement predictable even while looking up/down), mouse-look
// while the right mouse button is held. Deliberately minimal - no
// acceleration/smoothing/collision, matching this whole first pass's "just
// prove the pipeline works" scope.
struct FlyCamera {
    DirectX::XMFLOAT3 position{0.0f, 15.0f, -15.0f};
    float yaw = 0.0f;    // radians, rotation around the world Y (up) axis
    float pitch = -0.3f; // radians, look up/down; clamped in update() to avoid gimbal flip
    float moveSpeed = 15.0f;         // world units/second (SWG world units are ~meters)
    float mouseSensitivity = 0.005f; // radians per pixel of mouse movement

    // Reads input from `window` and updates position/yaw/pitch in place.
    // `deltaSeconds` is the real elapsed time since the previous call - call
    // exactly once per rendered frame.
    void update(Window& window, float deltaSeconds);

    DirectX::XMFLOAT3 forward() const;
    DirectX::XMMATRIX viewMatrix() const;
};

// A third-person camera locked to a moving target (self): orbits the target
// at `distance`, controlled by the same hold-right-mouse-to-look convention
// as FlyCamera, plus mouse-wheel zoom. No WASD - position is entirely
// derived from the target's position each frame, not free-flown.
struct FollowCamera {
    float yaw = 0.0f;
    float pitch = -0.25f;
    float distance = 8.0f;
    float minDistance = 2.0f;
    float maxDistance = 60.0f;
    float mouseSensitivity = 0.005f;
    float zoomPerWheelNotch = 0.75f; // world units per WHEEL_DELTA (120)
    // Looks at a point above the target's own position (its feet/base, per
    // WorldObject::x/y/z) rather than the target itself, so the camera
    // frames roughly chest-height instead of staring at the ground.
    float targetHeightOffset = 1.0f;

    DirectX::XMFLOAT3 position{}; // computed by update(); read-only afterward

    // Reads input from `window` and recomputes `position` relative to
    // `targetPosition` (the followed object's current world position, read
    // fresh every frame - this camera never caches its own world position
    // independent of the target). `deltaSeconds` is unused today (no
    // smoothing/lag on this first pass) but kept for signature symmetry with
    // FlyCamera::update() and in case smoothing is added later.
    void update(Window& window, const DirectX::XMFLOAT3& targetPosition, float deltaSeconds);

    // The direction from the camera toward the target (same spherical
    // yaw/pitch convention as FlyCamera::forward()) - exposed so callers can
    // derive a camera-facing billboard basis (right/up) without duplicating
    // the trig here.
    DirectX::XMFLOAT3 lookDirection() const;

    DirectX::XMMATRIX viewMatrix(const DirectX::XMFLOAT3& targetPosition) const;
};

// A view frustum, extracted from a combined view*projection matrix via
// Gribb/Hartmann plane extraction - adapted specifically for DirectXMath's
// row-vector convention (clip = v * M, v a row vector, so each clip
// component is a dot product with a COLUMN of M, and each plane is
// therefore a linear combination of the matrix's own COLUMNS, not rows)
// and Direct3D's [0,1] depth range (the near plane is `column2` alone, not
// `column3+column2` as in the OpenGL [-1,1]-depth version of this
// algorithm). Used for real portal-based cell
// visibility culling (see worldmodel::computeVisibleCells()) - this
// project's first frustum/culling infrastructure. Original code, not a
// port: no real client/server source describes this (it's generic camera
// math, not a real file format or protocol detail this project holds to
// its usual "verify against real bytes" bar).
struct Frustum {
    // Left, right, bottom, top, near, far, in that order - each plane is
    // (a, b, c, d) for ax+by+cz+d=0, normalized, with an INWARD-facing
    // normal (a point is on the inside of a plane when
    // a*x + b*y + c*z + d >= 0).
    std::array<DirectX::XMFLOAT4, 6> planes;

    static Frustum fromViewProjection(DirectX::XMMATRIX viewProj);

    // True if the sphere is at least partially inside every plane - the
    // standard conservative sphere-frustum test (may return true for a
    // sphere that's actually just outside a frustum corner - the two
    // "outside" regions of adjacent planes overlapping near a corner is a
    // well-known limitation of per-plane sphere tests - but never false for
    // one that's actually at least partially visible).
    bool intersectsSphere(DirectX::XMFLOAT3 center, float radius) const;
};

} // namespace renderer
