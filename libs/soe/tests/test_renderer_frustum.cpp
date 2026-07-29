// Verification for renderer::Frustum (Phase 22, real portal-based cell
// visibility culling) - Gribb/Hartmann plane extraction from a real
// DirectXMath row-vector view*projection matrix, and the sphere-vs-frustum
// test built on it. Original code (generic camera math, not a real file
// format), so this test is a from-first-principles geometric check against
// a known, hand-computable perspective matrix, not a real-bytes pin.
#include <doctest/doctest.h>

#include "renderer/Camera.h"

using namespace DirectX;
using namespace renderer;

namespace {
// Identity view (camera at the world origin, looking down +Z - DirectX's
// left-handed convention), fovY=90deg, aspect=1, near=1, far=100. At
// fovY=90deg the half-angle is 45deg, so at any depth z the visible
// half-width/half-height both equal z (tan(45deg) == 1) - makes expected
// in/out points easy to compute by hand.
Frustum makeTestFrustum() {
    XMMATRIX view = XMMatrixIdentity();
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 1.0f, 100.0f);
    return Frustum::fromViewProjection(XMMatrixMultiply(view, projection));
}
} // namespace

TEST_CASE("Frustum::intersectsSphere - a point well within near/far and the side planes is inside") {
    Frustum f = makeTestFrustum();
    CHECK(f.intersectsSphere(XMFLOAT3{0.0f, 0.0f, 50.0f}, 0.0f));
}

TEST_CASE("Frustum::intersectsSphere - a point closer than the near plane is outside") {
    Frustum f = makeTestFrustum();
    CHECK_FALSE(f.intersectsSphere(XMFLOAT3{0.0f, 0.0f, 0.5f}, 0.0f));
}

TEST_CASE("Frustum::intersectsSphere - a point beyond the far plane is outside") {
    Frustum f = makeTestFrustum();
    CHECK_FALSE(f.intersectsSphere(XMFLOAT3{0.0f, 0.0f, 150.0f}, 0.0f));
}

TEST_CASE("Frustum::intersectsSphere - a point well outside the left/right planes is outside") {
    Frustum f = makeTestFrustum();
    // At z=50, half-width is 50 (see makeTestFrustum's own comment) - x=200
    // is far outside it.
    CHECK_FALSE(f.intersectsSphere(XMFLOAT3{200.0f, 0.0f, 50.0f}, 0.0f));
}

TEST_CASE("Frustum::intersectsSphere - a point well outside the top/bottom planes is outside") {
    Frustum f = makeTestFrustum();
    CHECK_FALSE(f.intersectsSphere(XMFLOAT3{0.0f, 200.0f, 50.0f}, 0.0f));
}

TEST_CASE("Frustum::intersectsSphere - a point just outside a plane becomes visible once the "
          "sphere's own radius reaches it") {
    Frustum f = makeTestFrustum();
    // At z=50, x=60 is 10 units past the right edge (half-width 50).
    CHECK_FALSE(f.intersectsSphere(XMFLOAT3{60.0f, 0.0f, 50.0f}, 5.0f));  // radius too small to reach
    CHECK(f.intersectsSphere(XMFLOAT3{60.0f, 0.0f, 50.0f}, 15.0f));      // radius reaches back in
}
