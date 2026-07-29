// Verification for worldmodel::computeVisibleCells (Phase 22, real
// portal-based cell visibility culling) - the bounded BFS portal-graph walk
// itself, using hand-built cell/portal fixtures (no real .pob file needed -
// the algorithm only depends on the real CellPortal/PortalShape SHAPE, not
// any specific building's content) and a real renderer::Frustum for the
// visibility predicate, matching how the real call site
// (tools/dummyclient/Visualizer.cpp) wires the two together.
#include <doctest/doctest.h>

#include "renderer/Camera.h"
#include "worldmodel/PortalVisibility.h"

using namespace DirectX;
using namespace worldmodel;

namespace {

// Camera at the world origin, fovY=90deg (half-width/half-height == depth,
// same reasoning as test_renderer_frustum.cpp's own makeTestFrustum()),
// looking down +Z or -Z depending on `facingPositiveZ`.
renderer::Frustum makeFrustum(bool facingPositiveZ) {
    XMMATRIX view = facingPositiveZ ? XMMatrixIdentity() : XMMatrixRotationY(XM_PI);
    XMMATRIX projection = XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, 1.0f, 1000.0f);
    return renderer::Frustum::fromViewProjection(XMMatrixMultiply(view, projection));
}

auto visibilityPredicateFor(const renderer::Frustum& frustum) {
    return [&frustum](assets::Float3 center, float radius) {
        return frustum.intersectsSphere(XMFLOAT3{center.x, center.y, center.z}, radius);
    };
}

assets::PortalShape squarePortalAt(float x, float y, float z, float halfSize = 1.0f) {
    assets::PortalShape shape;
    shape.vertices = {
        {x - halfSize, y - halfSize, z},
        {x + halfSize, y - halfSize, z},
        {x + halfSize, y + halfSize, z},
        {x - halfSize, y + halfSize, z},
    };
    return shape;
}

} // namespace

TEST_CASE("computeVisibleCells - a single cell with no portals is visible from itself") {
    std::vector<std::vector<assets::CellPortal>> cellPortals(1);
    std::vector<assets::PortalShape> portalShapes;
    renderer::Frustum frustum = makeFrustum(/*facingPositiveZ=*/true);

    auto visible = computeVisibleCells(cellPortals, portalShapes, 0, visibilityPredicateFor(frustum));
    REQUIRE(visible.size() == 1);
    CHECK(visible[0]);
}

TEST_CASE("computeVisibleCells - startCellIndex out of range returns an all-false vector") {
    std::vector<std::vector<assets::CellPortal>> cellPortals(2);
    std::vector<assets::PortalShape> portalShapes;
    renderer::Frustum frustum = makeFrustum(true);

    auto visible = computeVisibleCells(cellPortals, portalShapes, 5, visibilityPredicateFor(frustum));
    REQUIRE(visible.size() == 2);
    CHECK_FALSE(visible[0]);
    CHECK_FALSE(visible[1]);
}

TEST_CASE("computeVisibleCells - two rooms: adjacent room visible through a portal facing the "
          "camera, not visible facing away") {
    // Room 0's portal to room 1 sits 10 units directly ahead.
    std::vector<std::vector<assets::CellPortal>> cellPortals = {
        {assets::CellPortal{/*portalShapeIndex=*/0, /*adjacentCellIndex=*/1}},
        {},
    };
    std::vector<assets::PortalShape> portalShapes = {squarePortalAt(0.0f, 0.0f, 10.0f)};

    SUBCASE("facing the portal") {
        renderer::Frustum frustum = makeFrustum(/*facingPositiveZ=*/true);
        auto visible =
            computeVisibleCells(cellPortals, portalShapes, 0, visibilityPredicateFor(frustum));
        REQUIRE(visible.size() == 2);
        CHECK(visible[0]);
        CHECK(visible[1]);
    }

    SUBCASE("facing away from the portal") {
        renderer::Frustum frustum = makeFrustum(/*facingPositiveZ=*/false);
        auto visible =
            computeVisibleCells(cellPortals, portalShapes, 0, visibilityPredicateFor(frustum));
        REQUIRE(visible.size() == 2);
        CHECK(visible[0]); // the starting cell is always visible
        CHECK_FALSE(visible[1]);
    }
}

TEST_CASE("computeVisibleCells - three-room chain: recurses through a second portal that's also "
          "in view, stops recursing when it isn't") {
    // Room 0 -> room 1 via a centered portal at z=10 (always in view of a
    // camera looking down +Z, since a centered point stays centered at any
    // depth). Room 1 -> room 2's own portal placement differs per SUBCASE.
    std::vector<std::vector<assets::CellPortal>> cellPortals = {
        {assets::CellPortal{0, 1}},
        {assets::CellPortal{1, 2}},
        {},
    };
    renderer::Frustum frustum = makeFrustum(/*facingPositiveZ=*/true);

    SUBCASE("room 1's own portal to room 2 is also centered/in view - recurses all the way") {
        std::vector<assets::PortalShape> portalShapes = {
            squarePortalAt(0.0f, 0.0f, 10.0f),
            squarePortalAt(0.0f, 0.0f, 20.0f),
        };
        auto visible =
            computeVisibleCells(cellPortals, portalShapes, 0, visibilityPredicateFor(frustum));
        REQUIRE(visible.size() == 3);
        CHECK(visible[0]);
        CHECK(visible[1]);
        CHECK(visible[2]); // reached via recursion through room 1's own portal
    }

    SUBCASE("room 1's own portal to room 2 is far off to the side - room 2 not reached") {
        std::vector<assets::PortalShape> portalShapes = {
            squarePortalAt(0.0f, 0.0f, 10.0f),
            squarePortalAt(5000.0f, 0.0f, 20.0f), // way outside the frustum's side planes
        };
        auto visible =
            computeVisibleCells(cellPortals, portalShapes, 0, visibilityPredicateFor(frustum));
        REQUIRE(visible.size() == 3);
        CHECK(visible[0]);
        CHECK(visible[1]); // reached - its OWN portal (0) was in view
        CHECK_FALSE(visible[2]); // not reached - room 1's portal to it was never in view
    }
}

TEST_CASE("computeVisibleCells - maxDepth bounds a long chain even when every portal is in view") {
    constexpr int kRoomCount = 10;
    std::vector<std::vector<assets::CellPortal>> cellPortals(kRoomCount);
    std::vector<assets::PortalShape> portalShapes;
    for (int i = 0; i < kRoomCount - 1; ++i) {
        cellPortals[static_cast<size_t>(i)].push_back(
            assets::CellPortal{static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1)});
        // Centered, so every portal is trivially in view of a camera at the
        // origin looking down +Z, regardless of depth.
        portalShapes.push_back(squarePortalAt(0.0f, 0.0f, static_cast<float>((i + 1) * 10)));
    }
    renderer::Frustum frustum = makeFrustum(/*facingPositiveZ=*/true);

    auto visible = computeVisibleCells(cellPortals, portalShapes, 0, visibilityPredicateFor(frustum),
                                        /*maxDepth=*/3);
    REQUIRE(visible.size() == kRoomCount);
    // Depth 0 (room 0) through depth 3 (room 3) reached; depth 4+ not.
    for (int i = 0; i <= 3; ++i) {
        CHECK(visible[static_cast<size_t>(i)]);
    }
    for (int i = 4; i < kRoomCount; ++i) {
        CHECK_FALSE(visible[static_cast<size_t>(i)]);
    }
}
